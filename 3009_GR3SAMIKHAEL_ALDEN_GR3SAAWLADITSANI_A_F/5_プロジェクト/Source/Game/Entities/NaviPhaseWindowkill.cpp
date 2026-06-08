#include "NaviPhaseWindowkill.h"
#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include <algorithm>
#include <random>
#include <SceneBoss.h>
#include "Framework.h"
#include "WindowManager.h" 
#include <SDL3/SDL.h>
#include <System/AudioManager.h>
#include "EffectManager.h"
#include "CameraController.h"

using namespace DirectX;

NaviPhaseWindowkill::NaviPhaseWindowkill(Player* player) {
    m_aiTarget = player;
}
// =========================================================
// [MAGIC] MEMORY MANAGEMENT & SPAWNING
// =========================================================
void NaviPhaseWindowkill::Enter(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    EffectManager::Instance().StopAll();

    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();
        SDL_SetWindowBordered(sdlWin, true);   // Munculkan border lagi
        SDL_SetWindowResizable(sdlWin, true); // Izinkan resize
    }

    auto device = Graphics::Instance().GetDevice();
    auto windowSystem = boss->GetWindowSystem();

    m_solidRenderer = std::make_unique<Primitive>(device);
    m_screenW = (float)GetSystemMetrics(SM_CXSCREEN);
    m_screenH = (float)GetSystemMetrics(SM_CYSCREEN);

    // Ciptakan jendela FX SECARA DINAMIS hanya ketika fase ini dimulai!
    TrackedWindowConfig fxCfg = { "navi_fx", "N.A.V.I - FX", (int)m_screenW, (int)m_screenH, 3 };
    fxCfg.role = WindowRole::SUB_VIEWPORT;
    fxCfg.isTransparent = true;

    windowSystem->AddTrackedWindow(fxCfg,
        []() { return DirectX::XMFLOAT3(0, 0, 0); },
        [this]() { return DirectX::XMFLOAT2(m_screenW, m_screenH); }
    );

    auto* fxWin = windowSystem->GetTrackedWindow("navi_fx");
    if (fxWin) {
        m_fxWindow = fxWin->window;
        m_fxCamera = fxWin->camera;
        m_fxWindow->SetBackgroundAlpha(0.0f);
        m_fxWindow->SetClickThrough(true);
        m_fxWindow->SetBorderVisible(false);
        m_fxWindow->SetDraggable(false);
    }

    // Load tekstur sayap ke VRAM hanya di fase ini
    m_wingSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]ErrorAtlas.png");
    m_placeholderModel = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");
    m_overdriveSprite = std::make_unique<Sprite>(device, "Data/Sprite/Boss/txt_nagaoshi.png");

    // Reset timer dan ciptakan array
    m_glitchTimer = 0.0f;
    m_wingState = WingState::Expanding;
    m_wingStateTimer = 0.0f;
    GenerateButterflyWings();

    boss->SetPosition({ 0.0f, 0.0f, 7.0f });
    boss->GetFaceParams().gridResolution = 16;
    boss->InitializeFaceGrid(Graphics::Instance().GetDevice());

    EffectManager::Instance().PreloadEffect(m_blasterParams.chargeEffectPath);
    EffectManager::Instance().PreloadEffect(m_blasterParams.fireEffectPath);

    m_bossRef = boss; // Simpan referensi boss untuk dipakai saat jendela hancur
    m_bossMaxHP = 7000;
    m_bossHP = m_bossMaxHP;
    m_hitFlashTimer = 0.0f;
    m_aiEnabled = false;
    m_aiGlobalCooldown = 1.0f;
    m_cdBouncing = 1.0f;
    m_cdBoomerang = 7.0f;
    m_cdOrbitalBlaster = 4.0f;
    m_cdTargetedBlaster = 10.0f;
    m_cdUndyne = 13.0f;

    if (windowSystem->GetTrackedWindow("player")) {
        windowSystem->RemoveTrackedWindow("player");
    }

    // =========================================================
    // [BARU] INISIALISASI KANDANG PEMAIN
    // =========================================================
    if (m_aiTarget) {
        m_isPlayerCaged = true;
        m_aiTarget->SetPosition(0, 0, -8.0f);
        m_cageMaxHP = 600; // Sesuaikan dengan total HP yang kamu inginkan
        m_cageHP = m_cageMaxHP;
        m_cagePos = m_aiTarget->GetPosition(); // Kunci posisi kandang di lokasi player saat ini
        m_cageWindowPos = m_cagePos;
        m_cageShakeTimer = 0.0f;
        m_aiTarget->SetShootDelay(0.0f);

        // Hitung ukuran asli di dunia 3D (300 pixel / ratio)
        m_cageSizeWorld = 300.0f / windowSystem->GetPixelToUnitRatio();

        TrackedWindowConfig cageCfg;
        cageCfg.name = m_cageWindowName;
        cageCfg.title = "TRAPPED.exe";
        cageCfg.width = 300;
        cageCfg.height = 300;
        cageCfg.role = WindowRole::TRACKED_ENTITY;
        cageCfg.priority = 100; // Pastikan selalu di atas
        cageCfg.isTransparent = false;

        windowSystem->AddTrackedWindow(cageCfg,
            [this]() { return m_cageWindowPos; }, // Posisinya STATIS, hanya jitter saat kena tembak
            []() { return DirectX::XMFLOAT2(300.0f, 300.0f); }
        );

        if (auto* cageWindow = windowSystem->GetTrackedWindow(m_cageWindowName)) {
            if (cageWindow->window) {
                cageWindow->window->SetBackgroundAlpha(1.0f);
            }
        }
    }

    m_overdriveAlpha = 0.0f;

    // =========================================================
    // [BARU] OPENING DIALOGUE — TRACKING WINDOW
    // Window ini mengambang mengikuti posisi m_dialogueWorldPos
    // (titik di atas/samping bos), transparan, no background.
    // Player tetap bisa bergerak — AI saja yang belum aktif.
    // =========================================================
    // Hitung offset agar window muncul CENTERED di atas bos.
    // Sistem tracking menaruh pojok kiri-atas window di posisi world,
    // jadi kita geser -halfWidth di X dan +sedikit di Z agar ada di atas bos.
    {
        float p2u = windowSystem->GetPixelToUnitRatio();
        float halfW = (m_dialogueWindowW * 0.5f) / p2u;
        // Bos ada di Z=7, window diletakkan sedikit di atasnya (Z lebih kecil = lebih ke atas layar)
        m_dialogueWorldPos = { -halfW, 0.0f, 1.5f };
    }

    {
        TrackedWindowConfig diagCfg;
        diagCfg.name = m_dialogueWindowName;
        diagCfg.title = "N.A.V.I";           // Judul window bar atas
        diagCfg.width = (int)m_dialogueWindowW;
        diagCfg.height = (int)m_dialogueWindowH;
        diagCfg.role = WindowRole::TRACKED_ENTITY;
        diagCfg.isTransparent = false;                // Solid — sama seperti kandang & bullet
        diagCfg.priority = 5;

        windowSystem->AddTrackedWindow(diagCfg,
            [this]() { return m_dialogueWorldPos; },
            [this]() { return DirectX::XMFLOAT2(m_dialogueWindowW, m_dialogueWindowH); }
        );

        auto* diagWin = windowSystem->GetTrackedWindow(m_dialogueWindowName);
        if (diagWin && diagWin->window) {
            m_dialogueWindow = diagWin->window;
            m_dialogueCamera = diagWin->camera;
            m_dialogueWindow->SetBackgroundAlpha(1.0f); // Background solid
            m_dialogueWindow->SetClickThrough(false);
            m_dialogueWindow->SetBorderVisible(true);   // Border OS tetap tampil
            m_dialogueWindow->SetDraggable(false);
        }
    }

    m_dialogueBox = std::make_unique<UIDialogueBox>();
    m_dialogueBox->Initialize();
    m_dialogueBox->SetShowBackground(false);  // No background — teks melayang di window transparan

    // Autoadvance ON, strict OFF → player bebas gerak, dialog jalan sendiri
    m_dialogueBox->SetAutoAdvance(false);
    m_dialogueBox->StartDialogue({
        u8"ウィンドウを撃て"
        });

    m_isDialogueActive = true;
    // AI belum aktif sampai dialog selesai, tapi input player TIDAK dikunci
    m_aiEnabled = false;
    if (m_aiTarget) {
        m_aiTarget->SetInputEnabled(true); // Player tetap bisa gerak dari awal
    }

    m_cageFirstHitTriggered = false;
    m_overdriveDialogueTriggered = false;
}

void NaviPhaseWindowkill::Exit(NaviBoss* boss) {
    m_deathCleanupDone = false;

    // 1. BERSIH-BERSIH WINDOW & TRACKING SYSTEM
    if (boss && boss->GetWindowSystem()) {
        boss->GetWindowSystem()->RemoveTrackedWindow("navi_fx");
        boss->GetWindowSystem()->RemoveTrackedWindow(m_dialogueWindowName);

        // Clean up the Player Cage if it still exists!
        boss->GetWindowSystem()->RemoveTrackedWindow(m_cageWindowName);

        for (auto& bwb : m_bouncingBullets) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
        }
        for (auto& b : m_blasters) {
            boss->GetWindowSystem()->RemoveTrackedWindow(b->beamWindowName);
            boss->GetWindowSystem()->RemoveTrackedWindow(b->windowName);
        }
        for (auto& bw : m_boomerangs) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
        }
        for (auto& spear : m_undyneSpears) {
            boss->GetWindowSystem()->RemoveTrackedWindow(spear.windowName);
        }
    }

    // 2. BERSIH-BERSIH EFEK PARTIKEL (VFX)
    for (auto& b : m_blasters) {
        EffectManager::Instance().Stop(b->chargeEffectHandle);
        EffectManager::Instance().Stop(b->fireEffectHandle); // Fixed: Stop the firing laser too
    }

    if (m_deathVfxHandle != -1) {
        EffectManager::Instance().Stop(m_deathVfxHandle);
        m_deathVfxHandle = -1;
    }

    // 3. RESET POINTER & STATE
    m_fxWindow = nullptr;
    m_fxCamera.reset();
    m_dialogueWindow = nullptr;
    m_dialogueCamera.reset();
    m_dialogueBox.reset();
    m_isDialogueActive = false;
    m_wingSprite.reset();
    m_leftWingData.clear();
    m_rightWingData.clear();
    m_overdriveSprite.reset();

    m_bouncingBullets.clear();
    m_blasters.clear();
    m_boomerangs.clear();
    m_undyneSpears.clear();

    // 4. KEMBALIKAN MAIN WINDOW KE STATE NORMAL
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_SetWindowAlwaysOnTop(mainWindow->GetSDLWindow(), false);
        mainWindow->SetPriority(50);
        WindowManager::Instance().MarkPriorityDirty();
    }
    m_deathWindowRaised = false;
}

// =========================================================
// WING LOGIC (Copas utuh dari versi sebelumnya)
// =========================================================
void NaviPhaseWindowkill::ReplayAnimation() {
    m_wingState = WingState::Expanding;
    m_wingStateTimer = 0.0f;
    m_wingFlickerTimer = 0.0f;
    if (m_fxWindow) m_fxWindow->SetFPSLimit(0.0f);
    GenerateButterflyWings();
}

void NaviPhaseWindowkill::GenerateButterflyWings() {
    m_leftWingData.clear();
    m_rightWingData.clear();
    std::mt19937 gen(m_wingSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const int NODES_PER_WING = 120;

    for (int i = 0; i < NODES_PER_WING; ++i) {
        float linearT = (float)i / NODES_PER_WING;
        float t = pow(linearT, 2.0f);
        float angle = t * DirectX::XM_PI;
        float rEdge = (exp(cos(angle)) - 2 * cos(4 * angle) - pow(sin(angle / 12), 5)) * 3.0f;
        float randomFill = pow(dist(gen), 0.3f);
        float r = (i % 5 == 0) ? rEdge : (rEdge * randomFill);
        float scatterX = (dist(gen) - 0.5f) * 1.5f;
        float scatterY = (dist(gen) - 0.5f) * 1.5f;

        WingNode node;
        node.localOffset = { 0.0f, 0.0f };
        node.targetOffset.y = (cos(angle) * r) + scatterY;
        node.flapOffset = t * 2.0f;

        float randomScale = 0.5f + dist(gen);
        float baseSize = 45.0f;
        node.size = { baseSize * randomScale, baseSize * randomScale };

        node.targetOffset.x = -abs(sin(angle) * r) + scatterX;
        m_leftWingData.push_back(node);
        node.targetOffset.x = abs(sin(angle) * r) - scatterX;
        m_rightWingData.push_back(node);
    }

    std::shuffle(m_leftWingData.begin(), m_leftWingData.end(), gen);
    std::shuffle(m_rightWingData.begin(), m_rightWingData.end(), gen);

    for (size_t i = 0; i < m_leftWingData.size(); ++i) {
        float linearDelay = ((float)i / m_leftWingData.size()) * m_spawnDuration;
        m_leftWingData[i].spawnDelay = max(0.0f, linearDelay + (dist(gen) - 0.5f) * m_spawnChaos);
    }
    for (size_t i = 0; i < m_rightWingData.size(); ++i) {
        float linearDelay = ((float)i / m_rightWingData.size()) * m_spawnDuration;
        m_rightWingData[i].spawnDelay = max(0.0f, linearDelay + (dist(gen) - 0.5f) * m_spawnChaos);
    }
}

void NaviPhaseWindowkill::Update(float dt, NaviBoss* boss) {
    m_glitchTimer += dt;

    // =========================================================
// [DEATH SEQUENCE] Boss Mati
// =========================================================
    if (m_bossHP <= 0) {
        if (!m_isDying) {
            m_isDying = true;
            m_deathTimer = 0.0f;

            // 1. Matikan AI dan hentikan semua serangan aktif
            m_aiEnabled = false;
            m_isSpawningBlasters = false;
            m_isSpawningTargetedBlasters = false;
            m_isSpawningBoomerangs = false;
            m_isSpawningBouncing = false;
            m_isSpawningUndynes = false;

            // 2. Mainkan VFX Kematian di posisi bos
            DirectX::XMFLOAT3 deathVfxPos = boss->GetPosition();
            deathVfxPos.y += 5.0f; // Offset Y — naikkan sesuai selera
            m_deathVfxHandle = EffectManager::Instance().Play(
                "Data/Effect/VFX_Boss_Death.efk", deathVfxPos, 2.0f);
            if (m_deathVfxHandle != -1) {
                float rotX = DirectX::XMConvertToRadians(90.0f);
                EffectManager::Instance().SetRotation(m_deathVfxHandle, { rotX, 0.0f, 0.0f });
            }

            // 3. Pre-load pecahan window agar tidak lag saat meledak nanti
            DirectX::XMFLOAT3 pos = boss->GetPosition();
        }

        m_deathTimer += dt;

        // VFX mengikuti posisi bos
        if (m_deathVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_deathVfxHandle)) {
            DirectX::XMFLOAT3 trackPos = boss->GetPosition();
            trackPos.y += 5.0f; // Sama dengan offset spawn
            EffectManager::Instance().SetPosition(m_deathVfxHandle, trackPos);
        }

        // 4. After 7 seconds — stop VFX and change the SCENE to SceneTitle
        if (m_deathTimer >= 7.0f) {
            if (!m_deathCleanupDone) {
                m_deathCleanupDone = true;

                if (m_deathVfxHandle != -1) {
                    EffectManager::Instance().Stop(m_deathVfxHandle);
                    m_deathVfxHandle = -1;
                }

                AudioManager::Instance().StopMusic();

                if (boss && boss->GetWindowSystem()) {
                    // Guard the remove — only remove if it actually exists
                    if (boss->GetWindowSystem()->GetTrackedWindow("navi_head")) {
                        boss->GetWindowSystem()->RemoveTrackedWindow("navi_head");
                    }
                }

                if (boss && boss->GetMainWindow() && boss->GetMainWindow()->GetSDLWindow()) {
                    SDL_HideWindow(boss->GetMainWindow()->GetSDLWindow());
                }
            }
            return;
        }



        // =========================================================
        // [BARU] Angkat main window ke paling depan sebagai
        // full-screen borderless renderer untuk model & efek
        // =========================================================
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
        if (mainWindow && mainWindow->GetSDLWindow()) {
            SDL_Window* sdlWin = mainWindow->GetSDLWindow();
            SDL_ShowWindow(sdlWin);
            SDL_SetWindowBordered(sdlWin, false);
            SDL_SetWindowResizable(sdlWin, false);
            SDL_SetWindowPosition(sdlWin, 0, 0);
            SDL_SetWindowSize(sdlWin, screenW, screenH + 1); // +1 anti exclusive fullscreen
            SDL_SetWindowAlwaysOnTop(sdlWin, true);

            mainWindow->SetPriority(0); // Prioritas tertinggi — di atas semua window OS
            WindowManager::Instance().MarkPriorityDirty();
        }

        // Turunkan prioritas window FX & tracking agar tidak menutupi main window
        if (m_fxWindow) {
            m_fxWindow->SetPriority(10);
            WindowManager::Instance().MarkPriorityDirty();
        }

        m_deathWindowRaised = true;
    }

    if (m_hitFlashTimer > 0.0f) {
        m_hitFlashTimer -= dt;
        if (m_hitFlashTimer < 0.0f) m_hitFlashTimer = 0.0f;
    }

    if (m_isDialogueActive && m_dialogueBox) {
        // Tetap panggil Update agar jika ada animasi teks, dia tetap jalan
        m_dialogueBox->Update(dt);

        // KITA HAPUS 'else' otomatis. 
        // Dialog hanya ditutup jika kondisi pemain (Cage hancur/Overdrive) terpenuhi.
        if (!m_isPlayerCaged || m_overdriveAlpha > 0.0f) {
            m_isDialogueActive = false;

            // Bersihkan window dari sistem tracking
            if (boss && boss->GetWindowSystem()) {
                boss->GetWindowSystem()->RemoveTrackedWindow(m_dialogueWindowName);
            }

            // Cleanup pointer
            m_dialogueWindow = nullptr;
            m_dialogueCamera.reset(); // <--- WAJIB ada agar camera bersih
            m_dialogueBox.reset();

            // Aktifkan AI setelah misi selesai
            m_aiEnabled = (m_aiTarget != nullptr);
        }
    }

    if (m_bossHP <= 0) {
        m_aiEnabled = false;
    }

    m_cageWindowPos = m_cagePos;
    if (m_isPlayerCaged && m_cageShakeTimer > 0.0f) {
        m_cageShakeTimer -= dt;
        float t = (m_cageShakeDuration > 0.0f) ? (m_cageShakeTimer / m_cageShakeDuration) : 0.0f;
        float strength = max(0.0f, t) * m_cageShakeIntensity;
        float offsetX = (((rand() % 200) / 100.0f) - 1.0f) * strength;
        float offsetZ = (((rand() % 200) / 100.0f) - 1.0f) * strength;
        m_cageWindowPos = { m_cagePos.x + offsetX, m_cagePos.y, m_cagePos.z + offsetZ };
    }

    // =========================================================
    // [BARU] ANIMASI FADE-IN OVERDRIVE SPRITE
    // =========================================================
    // Syarat: Player sudah uncapped DAN kandang masih belum hancur
    if (m_aiTarget && m_aiTarget->IsPowerUncapped() && m_isPlayerCaged) {
        m_overdriveAlpha += m_overdriveFadeSpeed * dt;
        if (m_overdriveAlpha > 1.0f) m_overdriveAlpha = 1.0f;

        // ---- One-shot: overdrive aktif, dialogue tidak dibutuhkan ----
        if (!m_overdriveDialogueTriggered) {
            m_overdriveDialogueTriggered = true;

            // Tutup window dialogue lama jika masih ada
            if (m_isDialogueActive && boss && boss->GetWindowSystem()) {
                boss->GetWindowSystem()->RemoveTrackedWindow(m_dialogueWindowName);
                m_dialogueWindow = nullptr;
                m_dialogueCamera.reset();
                m_isDialogueActive = false;
            }
        }
    }
    else {
        // Jika kandang hancur (atau belum overdrive), alpha dikembalikan ke 0 (menghilang)
        m_overdriveAlpha = 0.0f;
    }

    // =========================================================
        // [FIX] KURUNG PEMAIN DI DALAM KANDANG (HITBOX & VISUAL SINKRON)
        // =========================================================
    if (m_isPlayerCaged && m_aiTarget) {
        DirectX::XMFLOAT3 pPos = m_aiTarget->GetPosition();

        float halfSize = (m_cageSizeWorld * 0.5f) - 0.5f;
        bool isClamped = false; // Penanda jika pemain mentok

        // Tembok Kiri Kanan (Sumbu X)
        if (pPos.x > m_cagePos.x + halfSize) { pPos.x = m_cagePos.x + halfSize; isClamped = true; }
        if (pPos.x < m_cagePos.x - halfSize) { pPos.x = m_cagePos.x - halfSize; isClamped = true; }

        // Tembok Atas Bawah (Sumbu Z)
        if (pPos.z > m_cagePos.z + halfSize) { pPos.z = m_cagePos.z + halfSize; isClamped = true; }
        if (pPos.z < m_cagePos.z - halfSize) { pPos.z = m_cagePos.z - halfSize; isClamped = true; }

        if (isClamped) {
            // Paksa pergerakan fisika
            m_aiTarget->GetMovement()->SetPosition(pPos);
            // [BARU] Paksa juga posisi model/entitas agar tidak nge-drift keluar!
            m_aiTarget->SetPosition(pPos);

            // Opsional: Hentikan velocity agar pemain tidak "meluncur" di dinding
            // m_aiTarget->GetMovement()->SetVelocity({0,0,0}); 
        }
    }

    // =========================================================
    // [NEW] GERAKAN MENGAMBANG (IDLE HOVER) BOS NAVI
    // =========================================================
    if (boss) {
        float centerX = 0.0f;
        float centerZ = 7.0f;

        // Atur seberapa jauh bos boleh menyimpang dari titik tengah
        float rangeX = 2.0f;
        float rangeZ = 1.0f;

        // Kombinasi sin dan cos dengan pengali waktu yang berbeda 
        // agar polanya tidak membentuk lingkaran sempurna yang membosankan.
        float newX = centerX + sinf(m_glitchTimer * 0.6f) * rangeX;
        float newZ = centerZ + cosf(m_glitchTimer * 0.4f) * rangeZ;

        // Terapkan posisi baru! (Sumbu Y tetap 0 agar tidak naik-turun)
        boss->SetPosition({ newX, 0.0f, newZ });
    }

    UpdateAI(dt, boss);

    if (m_wingState == WingState::Expanding) {
        m_wingStateTimer += dt;
        if (m_wingStateTimer >= WING_EXPAND_DURATION) {
            m_wingState = WingState::Idle;
            if (m_fxWindow) m_fxWindow->SetFPSLimit(0.0f);
        }
    }
    else {
        m_wingFlickerTimer += dt;
        if (m_wingFlickerTimer >= m_nextFlickerTarget) {
            m_wingFlickerTimer = 0.0f;
            float randomFactor = (rand() % 100) / 100.0f;
            m_nextFlickerTarget = 0.05f + (1.0f - min(1.0f, m_spawnChaos)) * 0.8f * randomFactor;
            int windowsToClose = 1 + (rand() % (int)(max(1.0f, m_spawnChaos * 5.0f)));

            for (int w = 0; w < windowsToClose; ++w) {
                std::vector<WingNode>& targetWing = (rand() % 2 == 0) ? m_leftWingData : m_rightWingData;
                if (targetWing.size() > 20) {
                    targetWing[rand() % (targetWing.size() - 10)].isClosing = true;
                }
            }
        }
    }

    auto updateNodeAnimations = [&](std::vector<WingNode>& wingData) {
        int nodeToMoveToTop = -1;
        for (int i = 0; i < wingData.size(); ++i) {
            auto& node = wingData[i];
            if (m_wingState == WingState::Expanding) {
                if (m_wingStateTimer >= node.spawnDelay) {
                    node.animScale = min(1.0f, node.animScale + dt / m_popDuration);
                }
            }
            else {
                if (node.isClosing) {
                    node.animScale -= dt / m_popDuration;
                    if (node.animScale <= 0.0f) {
                        node.animScale = 0.0f;
                        node.isClosing = false;
                        nodeToMoveToTop = i;
                    }
                }
                else if (node.animScale < 1.0f) {
                    float individualSpeed = m_popDuration * (0.8f + ((rand() % 40) / 100.0f));
                    node.animScale = min(1.0f, node.animScale + dt / individualSpeed);
                }
            }
        }
        if (nodeToMoveToTop != -1) {
            WingNode temp = wingData[nodeToMoveToTop];
            wingData.erase(wingData.begin() + nodeToMoveToTop);
            wingData.push_back(temp);
        }
        };

    updateNodeAnimations(m_leftWingData);
    updateNodeAnimations(m_rightWingData);

    for (auto& node : m_leftWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;
        node.localOffset.x -= sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
    }

    for (auto& node : m_rightWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;
        node.localOffset.x += sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
    }

    // =========================================================
    // UPDATE BOUNCING BULLETS & WINDOWS
    // =========================================================
    float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
    float limitX = (m_screenW / 2.0f) / p2u;
    float limitZ = (m_screenH / 2.0f) / p2u;

    bool anyBouncedThisFrame = false;

    for (auto it = m_bouncingBullets.begin(); it != m_bouncingBullets.end(); ) {
        auto& bwb = *it;

        if (!bwb.bullet->IsActive()) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
            it = m_bouncingBullets.erase(it);
            continue;
        }

        // =========================================================
        // [FIX] TERAPKAN SLIDER IMGUI SECARA REAL-TIME KE GAME!
        // =========================================================
        bwb.bullet->SetRadius(m_bouncingParams.hitboxRadius);
        bwb.bullet->SetDamage(m_bouncingParams.damage);
        float vs = m_bouncingParams.visualScale;
        bwb.bullet->scale = { vs, vs, vs };

        bwb.bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 pos = bwb.bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 vel = bwb.bullet->GetVelocity();
        float radius = bwb.bullet->GetRadius();

        // Logika Pantulan
        if (bwb.bounceCount < bwb.maxBounces) {
            bool bounced = false;
            if (pos.x > limitX - radius) { pos.x = limitX - radius; vel.x *= -1.0f; bounced = true; }
            else if (pos.x < -limitX + radius) { pos.x = -limitX + radius; vel.x *= -1.0f; bounced = true; }

            if (pos.z > limitZ - radius) { pos.z = limitZ - radius; vel.z *= -1.0f; bounced = true; }
            else if (pos.z < -limitZ + radius) { pos.z = -limitZ + radius; vel.z *= -1.0f; bounced = true; }

            if (bounced) {
                bwb.bounceCount++;
                bwb.bullet->ApplyMovement(pos, vel);
                anyBouncedThisFrame = true;
            }
        }
        else {
            // Pantulan habis, hapus window saat keluar layar
            if (abs(pos.x) > limitX + 15.0f || abs(pos.z) > limitZ + 15.0f) {
                boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
                it = m_bouncingBullets.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (anyBouncedThisFrame) {
        CameraController::Instance().AddTrauma(0.3f);
        std::string dashSounds[] = {
            "Data/Sound/SE_Boss_Bouncing_Thud_01.wav",
            "Data/Sound/SE_Boss_Bouncing_Thud_02.wav",
            "Data/Sound/SE_Boss_Bouncing_Thud_03.wav"
        };

        // 2. Pilih index secara acak (0, 1, atau 2)
        int randomIndex = rand() % 3;

        // 3. Mainkan suaranya lewat AudioManager
        // Kita gunakan volume 0.5f agar tidak terlalu memekakkan telinga
        AudioManager::Instance().PlaySFX(dashSounds[randomIndex], 0.2f);


    }

    // =========================================================
    // SPAWNER BOUNCING WINDOWS BERUNTUN (DENGAN POLA ASIMETRIS)
    // =========================================================
    if (m_isSpawningBouncing) {
        m_bouncingSpawnTimer += dt;

        while (m_isSpawningBouncing && m_bouncingSpawnTimer >= m_bouncingParams.spawnDelay) {
            if (m_bouncingParams.spawnDelay > 0.0f) m_bouncingSpawnTimer -= m_bouncingParams.spawnDelay;
            else m_bouncingSpawnTimer = 1.0f;

            // 1. Array Pola Asimetris (2 Kanan, 1 Kiri)
            float patternAngles[3] = {
                DirectX::XMConvertToRadians(35.0f),
                DirectX::XMConvertToRadians(75.0f),
                DirectX::XMConvertToRadians(145.0f)
            };

            BouncingWindowBullet bwb;
            bwb.bullet = std::make_unique<Bullet>();
            bwb.bullet->SetRadius(m_bouncingParams.hitboxRadius);
            bwb.bullet->SetDamage(m_bouncingParams.damage);
            float vs = m_bouncingParams.visualScale;
            bwb.bullet->scale = { vs, vs, vs };
            bwb.maxBounces = m_bouncingParams.maxBounces;

            // 2. Ambil sudut sesuai urutan peluru yang sedang di-spawn
            float angle = patternAngles[m_bouncingSpawned % 3];
            if (m_bouncingSpawned >= 3) {
                angle += DirectX::XMConvertToRadians((float)((rand() % 10) - 5));
            }

            DirectX::XMFLOAT3 dir = { cosf(angle), 0, sinf(angle) };
            bwb.bullet->Fire(boss->GetPosition(), dir, m_bouncingParams.speed);

            bwb.windowName = "bouncing_win_" + std::to_string(rand() % 100000);

            TrackedWindowConfig cfg;
            cfg.name = bwb.windowName;
            cfg.title = "PROJECTILE";
            cfg.width = (int)m_bouncingParams.windowWidth;
            cfg.height = (int)m_bouncingParams.windowHeight;
            cfg.isTransparent = false;

            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [ptr = bwb.bullet.get()]() { return ptr->GetPosition(); },
                [this]() { return DirectX::XMFLOAT2(m_bouncingParams.windowWidth, m_bouncingParams.windowHeight); }
            );

            m_bouncingBullets.push_back(std::move(bwb));
            m_bouncingSpawned++;

            // =========================================================
            // [JUICE] EFEK AUDIO & SHAKE SETIAP KALI SATU PELURU KELUAR!
            // =========================================================
            CameraController::Instance().AddTrauma(0.2f);
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Bouncing_Shoot.wav", 0.1f); // Sesuaikan nama file audio Anda

            if (m_bouncingSpawned >= m_bouncingParams.spawnCount) m_isSpawningBouncing = false;
        }
    }

    // =========================================================
        // 1. SPAWNER BOOMERANG BERUNTUN
        // =========================================================
    if (m_isSpawningBoomerangs) {
        m_boomerangSpawnTimer += dt;

        while (m_isSpawningBoomerangs && m_boomerangSpawnTimer >= m_boomerangParams.spawnDelay) {
            if (m_boomerangParams.spawnDelay > 0.0f) m_boomerangSpawnTimer -= m_boomerangParams.spawnDelay;
            else m_boomerangSpawnTimer = 1.0f;

            BoomerangWindowBullet bw;
            bw.bullet = std::make_unique<Bullet>();
            bw.bullet->SetRadius(m_boomerangParams.hitboxRadius);
            bw.bullet->SetDamage(m_boomerangParams.damage);
            bw.bullet->scale = { m_boomerangParams.visualScale, m_boomerangParams.visualScale, m_boomerangParams.visualScale };

            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            float limitX = (m_screenW / 2.0f) / p2u;
            float limitZ = (m_screenH / 2.0f) / p2u;

            // Tentukan arah acak (Kiri atau Kanan) untuk setiap Boomerang
            bw.spawnSide = (rand() % 2 == 0) ? 1 : -1;
            bw.startX = bw.spawnSide * (limitX + 8.0f); // Mulai dari luar layar

            // Target titik putar (Sekitar tengah layar agar ada ruang untuk overshoot)
            bw.targetX = bw.startX - (bw.spawnSide * m_boomerangParams.maxTravelDistance);

            // =========================================================
             // [FIX] LOGIKA PEMBATASAN AREA LAYER (FULL VS HALF-BOTTOM)
             // =========================================================
            float randomZ = 0.0f;
            if (m_boomerangParams.spawnBottomHalfOnly) {
                // Sumbu Z negatif memproyeksikan objek ke area setengah bawah monitor Anda
                randomZ = -((rand() % 100) / 100.0f) * (limitZ - 3.0f);
            }
            else {
                // Seluruh area monitor (Z positif atas, Z negatif bawah)
                randomZ = ((rand() % 200) / 100.0f - 1.0f) * (limitZ - 3.0f);
            }

            // Atur posisi awal objek dengan koordinat Z yang baru digenerate
            DirectX::XMFLOAT3 startPos = { bw.startX, 1.0f, randomZ };

            // Set Target Kecepatan awal (Masuk lurus)
            bw.targetVelX = -1.0f * bw.spawnSide * m_boomerangParams.speed;

            // Tembakkan dengan kecepatan awal penuh
            DirectX::XMFLOAT3 startDir = { bw.targetVelX > 0 ? 1.0f : -1.0f, 0.0f, 0.0f };
            bw.bullet->Fire(startPos, startDir, m_boomerangParams.speed);

            bw.windowName = "boomerang_win_" + std::to_string(rand() % 100000);

            TrackedWindowConfig cfg;
            cfg.name = bw.windowName;
            cfg.title = "BOOMERANG WARNING";
            cfg.width = (int)m_boomerangParams.windowSize;
            cfg.height = (int)m_boomerangParams.windowSize;
            cfg.isTransparent = false;
            cfg.priority = 8;

            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [ptr = bw.bullet.get()]() { return ptr->GetPosition(); },
                [this]() { return DirectX::XMFLOAT2(m_boomerangParams.windowSize, m_boomerangParams.windowSize); }
            );

            m_boomerangs.push_back(std::move(bw));
            m_boomerangsSpawned++;

            CameraController::Instance().AddTrauma(0.1f);

            if (m_boomerangsSpawned >= m_boomerangParams.spawnCount) m_isSpawningBoomerangs = false;
        }
    }

    // =========================================================
    // 2. UPDATE GERAK BOOMERANG (DENGAN LERP & EASE-IN/OUT)
    // =========================================================
    for (auto it = m_boomerangs.begin(); it != m_boomerangs.end(); ) {
        auto& bw = *it;

        if (!bw.bullet->IsActive()) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
            it = m_boomerangs.erase(it);
            continue;
        }

        bw.bullet->SetRadius(m_boomerangParams.hitboxRadius);
        bw.bullet->SetDamage(m_boomerangParams.damage);
        float vs = m_boomerangParams.visualScale;
        bw.bullet->scale = { vs, vs, vs };

        // [MAGIC FIX] Smooth Velocity Lerp!
        DirectX::XMFLOAT3 vel = bw.bullet->GetVelocity();
        // Kecepatan peluru secara dinamis mendekati targetVelX dengan kehalusan `turnSpeed`
        vel.x += (bw.targetVelX - vel.x) * m_boomerangParams.turnSpeed * dt;

        bw.bullet->ApplyMovement(bw.bullet->GetMovement()->GetPosition(), vel);
        bw.bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 pos = bw.bullet->GetMovement()->GetPosition();

        if (bw.state == 0) { // MODE MASUK
            // Saat mendekati titik target putar balik
            if ((bw.spawnSide == 1 && pos.x <= bw.targetX) ||
                (bw.spawnSide == -1 && pos.x >= bw.targetX))
            {
                bw.state = 1;
                // Set target kecepatan menjadi BERLAWANAN! 
                // (Ini akan membuat bumerang melambat sendiri secara halus, berhenti sejenak (overshoot), lalu ngebut mundur)
                bw.targetVelX = bw.spawnSide * m_boomerangParams.speed;
            }
        }
        else if (bw.state == 1) { // MODE KELUAR
            // Cek apakah sudah kembali ke titik awal di luar layar
            if ((bw.spawnSide == 1 && pos.x >= bw.startX) ||
                (bw.spawnSide == -1 && pos.x <= bw.startX))
            {
                boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
                it = m_boomerangs.erase(it);
                continue;
            }
        }
        ++it;
    }

    // =========================================================
        // 1. LOGIKA SPAWNER BLASTER BERUNTUN
        // =========================================================
    if (m_isSpawningBlasters) {
        m_blasterSpawnTimer += dt;

        // Loop ini akan menembak beruntun jika delay = 0
        while (m_isSpawningBlasters && m_blasterSpawnTimer >= m_blasterParams.spawnDelay) {
            if (m_blasterParams.spawnDelay > 0.0f) m_blasterSpawnTimer -= m_blasterParams.spawnDelay;
            else m_blasterSpawnTimer = 1.0f; // Mencegah infinite loop jika delay 0

            auto blaster = std::make_shared<OrbitalBlaster>();
            blaster->active = true;
            blaster->state = 1;
            blaster->timer = 0.0f;

            // Hitung Posisi X (Kiri ke Kanan)
            float startX = -m_blasterParams.spawnSpreadX * 0.5f;
            float stepX = (m_blasterParams.spawnCount > 1) ? (m_blasterParams.spawnSpreadX / (m_blasterParams.spawnCount - 1)) : 0.0f;

            blaster->baseX = startX + (stepX * m_blastersSpawned); // Simpan posisi asli
            blaster->pos = { blaster->baseX, 1.0f, 25.0f };
            blaster->targetPos = { blaster->baseX, 1.0f, 10.0f };
            blaster->beamScaleX = 0.0f;
            blaster->beamCurrentLength = 0.0f;

            int uniqueID = rand() % 100000;
            blaster->beamWindowName = "blaster_beam_" + std::to_string(uniqueID);
            blaster->windowName = "blaster_cannon_" + std::to_string(uniqueID);

            // A. SPAWN LASER WINDOW
            TrackedWindowConfig beamCfg;
            beamCfg.name = blaster->beamWindowName;
            beamCfg.title = "!!! BEAM REACHING !!!";
            beamCfg.isTransparent = false;
            beamCfg.priority = 1;

            boss->GetWindowSystem()->AddTrackedWindow(beamCfg,
                [ptr = blaster.get()]() {
                    if (ptr->state < 2) return DirectX::XMFLOAT3(ptr->pos.x, ptr->pos.y, -10000.0f);
                    return ptr->pos;
                },
                [ptr = blaster.get(), this, boss]() {
                    if (ptr->state < 2) return DirectX::XMFLOAT2(1.0f, 1.0f);
                    float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
                    float w = max(50.0f, (ptr->beamScaleX + 1.5f) * p2u);
                    float h = max(1.0f, ptr->beamCurrentLength * p2u);
                    return DirectX::XMFLOAT2(w, h);
                }
            );

            // B. SPAWN CANNON WINDOW
            TrackedWindowConfig cfg;
            cfg.name = blaster->windowName;
            cfg.title = "DANGER: ORBITAL CANNON";
            cfg.width = (int)m_blasterParams.cannonWindowSize;
            cfg.height = (int)m_blasterParams.cannonWindowSize;
            cfg.role = WindowRole::TRACKED_ENTITY;
            cfg.isTransparent = false;
            cfg.priority = 10;

            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [ptr = blaster.get()]() { return ptr->pos; },
                [this]() { return DirectX::XMFLOAT2(m_blasterParams.cannonWindowSize, m_blasterParams.cannonWindowSize); }
            );

            m_blasters.push_back(blaster);
            m_blastersSpawned++;

            if (m_blastersSpawned >= m_blasterParams.spawnCount) m_isSpawningBlasters = false;
        }
    }

    // =========================================================
    // 2. UPDATE PERILAKU SEMUA BLASTER AKTIF
    // =========================================================
    for (auto it = m_blasters.begin(); it != m_blasters.end(); ) {
        auto& b = *it;

        if (!b->active) {
            it = m_blasters.erase(it);
            continue;
        }

        b->timer += dt;

        float activeDropIn = b->isTargeted ? m_targetedBlasterParams.dropInDuration : m_blasterParams.dropInDuration;
        float activeCharge = b->isTargeted ? m_targetedBlasterParams.chargeDelay : m_blasterParams.chargeDelay;
        float activeFire = b->isTargeted ? m_targetedBlasterParams.fireDuration : m_blasterParams.fireDuration;

        // [PRO APPROACH] Hitung posisi efek satu kali saja untuk frame ini
        DirectX::XMFLOAT3 vfxPos = {
            b->pos.x + m_blasterParams.effectOffset.x,
            b->pos.y + m_blasterParams.effectOffset.y,
            b->pos.z + m_blasterParams.effectOffset.z
        };

        if (b->state == 1) { // DROP IN
            b->pos.z += (b->targetPos.z - b->pos.z) * 12.0f * dt;

            if (b->timer >= activeDropIn) {
                b->state = 2;
                b->timer = 0.0f;
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Laser_Charge.wav", 0.1f);

                // Play Charge Effect (TEST.efk)
                b->chargeEffectHandle = EffectManager::Instance().Play(
                    m_blasterParams.chargeEffectPath,
                    vfxPos,
                    m_blasterParams.chargeEffectScale
                );

                float rotX = DirectX::XMConvertToRadians(m_blasterParams.effectPitchDegrees);
                EffectManager::Instance().SetRotation(b->chargeEffectHandle, { rotX, 0.0f, 0.0f });
                EffectManager::Instance().SetTargetPosition(b->chargeEffectHandle, vfxPos);
            }
        }
        else if (b->state == 2) { // CHARGE
            b->beamScaleX = 0.2f;
            b->beamCurrentLength += (m_blasterParams.beamMaxLength - b->beamCurrentLength) * m_blasterParams.beamSlideSpeed * dt;

            // Tracking Charge Effect
            EffectManager::Instance().SetPosition(b->chargeEffectHandle, vfxPos);
            EffectManager::Instance().SetTargetPosition(b->chargeEffectHandle, vfxPos);

            if (b->timer >= activeCharge) {
                b->state = 3;
                b->timer = 0.0f;
                CameraController::Instance().AddTrauma(0.6f);
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Laser_Shoot.wav", 0.2f);

                // MATIKAN EFEK CHARGE!
                EffectManager::Instance().Stop(b->chargeEffectHandle);

                // MAIN KAN EFEK TEMBAKAN LASER (LASER.efk)
                b->fireEffectHandle = EffectManager::Instance().Play(
                    m_blasterParams.fireEffectPath,
                    vfxPos,
                    m_blasterParams.fireEffectScale
                );

                float rotX = DirectX::XMConvertToRadians(m_blasterParams.effectPitchDegrees);
                EffectManager::Instance().SetRotation(b->fireEffectHandle, { rotX, 0.0f, 0.0f });
                EffectManager::Instance().SetTargetPosition(b->fireEffectHandle, vfxPos);
            }
        }
        else if (b->state == 3) { // FIRE
            b->pos.x = b->baseX;
            b->beamScaleX += (m_blasterParams.beamVisualWidth - b->beamScaleX) * m_blasterParams.beamGrowSpeed * dt;
            CameraController::Instance().AddTrauma(0.1f);

            // Tracking Fire Effect
            EffectManager::Instance().SetPosition(b->fireEffectHandle, vfxPos);
            EffectManager::Instance().SetTargetPosition(b->fireEffectHandle, vfxPos);

            if (b->timer >= activeFire) {
                b->state = 4;
                b->timer = 0.0f;

                // MATIKAN EFEK TEMBAKAN LASER KARENA DURASI HABIS
                EffectManager::Instance().Stop(b->fireEffectHandle);
            }
        }
        else if (b->state == 4) { // POST FIRE / RETREAT
            b->beamScaleX -= dt * m_blasterParams.windowFadeSpeed;
            if (b->beamScaleX < 0.0f) b->beamScaleX = 0.0f;

            if (b->beamScaleX <= 0.1f) {
                if (boss && boss->GetWindowSystem() && boss->GetWindowSystem()->GetTrackedWindow(b->beamWindowName)) {
                    b->pos.z = -10000.0f;
                    boss->GetWindowSystem()->RemoveTrackedWindow(b->beamWindowName);
                }
            }

            b->pos.z += m_blasterParams.retreatSpeed * dt;

            if (b->timer >= 0.3f) {
                b->active = false;

                // Failsafe pembersihan memori jika state lompat atau error
                EffectManager::Instance().Stop(b->chargeEffectHandle);
                EffectManager::Instance().Stop(b->fireEffectHandle);

                if (boss && boss->GetWindowSystem()) {
                    boss->GetWindowSystem()->RemoveTrackedWindow(b->windowName);
                }
            }
        }
        ++it;
    }

    // =========================================================
        // [BARU] Logika Spawner Targeted Blaster (Hanya X-Axis)
        // =========================================================
    if (m_isSpawningTargetedBlasters) {
        m_targetedBlasterSpawnTimer += dt;

        while (m_isSpawningTargetedBlasters && m_targetedBlasterSpawnTimer >= m_targetedBlasterParams.spawnDelay) {
            if (m_targetedBlasterParams.spawnDelay > 0.0f) m_targetedBlasterSpawnTimer -= m_targetedBlasterParams.spawnDelay;
            else m_targetedBlasterSpawnTimer = 1.0f;

            // 1. Ambil posisi X player saja!
            float lockX = 0.0f;
            if (m_aiTarget) {
                lockX = m_aiTarget->GetPosition().x;
            }

            auto blaster = std::make_shared<OrbitalBlaster>();
            blaster->active = true;
            blaster->state = 1;
            blaster->timer = 0.0f;

            // [PENTING] Tandai bahwa ini blaster khusus!
            blaster->isTargeted = true;

            // 3. KUNCI POSISI X SAJA! Z menggunakan parameter fixedTargetZ
            blaster->baseX = lockX;
            blaster->pos = { lockX, 1.0f, m_targetedBlasterParams.fixedTargetZ + 15.0f };
            blaster->targetPos = { lockX, 1.0f, m_targetedBlasterParams.fixedTargetZ };

            blaster->beamScaleX = 0.0f;
            blaster->beamCurrentLength = 0.0f;
            int uniqueID = rand() % 100000;
            blaster->beamWindowName = "targeted_beam_" + std::to_string(uniqueID);
            blaster->windowName = "targeted_cannon_" + std::to_string(uniqueID);

            // A. SPAWN LASER WINDOW
            TrackedWindowConfig beamCfg;
            beamCfg.name = blaster->beamWindowName;
            beamCfg.title = "!!! TARGETED BEAM !!!";
            beamCfg.isTransparent = false;
            beamCfg.priority = 1;

            boss->GetWindowSystem()->AddTrackedWindow(beamCfg,
                [ptr = blaster.get()]() {
                    if (ptr->state < 2) return DirectX::XMFLOAT3(ptr->pos.x, ptr->pos.y, -10000.0f);
                    return ptr->pos;
                },
                [ptr = blaster.get(), this, boss]() {
                    if (ptr->state < 2) return DirectX::XMFLOAT2(1.0f, 1.0f);
                    float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
                    float w = max(50.0f, (ptr->beamScaleX + 1.5f) * p2u);
                    float h = max(1.0f, ptr->beamCurrentLength * p2u);
                    return DirectX::XMFLOAT2(w, h);
                }
            );

            // B. SPAWN CANNON WINDOW
            TrackedWindowConfig cfg;
            cfg.name = blaster->windowName;
            cfg.title = "DANGER: TARGETED CANNON";
            cfg.width = (int)m_blasterParams.cannonWindowSize;
            cfg.height = (int)m_blasterParams.cannonWindowSize;
            cfg.role = WindowRole::TRACKED_ENTITY;
            cfg.isTransparent = false;
            cfg.priority = 10;

            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [ptr = blaster.get()]() { return ptr->pos; },
                [this]() { return DirectX::XMFLOAT2(m_blasterParams.cannonWindowSize, m_blasterParams.cannonWindowSize); }
            );

            // 4. Masukkan ke list utama agar dieksekusi di blok "2. UPDATE PERILAKU"
            m_blasters.push_back(blaster);
            m_targetedBlastersSpawned++;

            if (m_targetedBlastersSpawned >= m_targetedBlasterParams.spawnCount) {
                m_isSpawningTargetedBlasters = false;
            }
        }
    }

    // =========================================================
        // 1. Logika Spawner (Formasi Pelangi Rapi di Bawah Layar)
        // =========================================================
    if (m_isSpawningUndynes) {
        m_undyneSpawnTimer -= dt;
        if (m_undyneSpawnTimer <= 0.0f) {
            m_undyneSpawnTimer = m_undyneParams.spawnDelay;

            float angleRange = m_undyneParams.arcMaxAngle - m_undyneParams.arcMinAngle;
            float step = (m_undyneParams.count > 1) ? (angleRange / (m_undyneParams.count - 1)) : 0.0f;

            // [MODIFIKASI BARU] Ambil indeks acak dari array!
            int randomPositionIndex = m_undyneSpawnIndices[m_undynesSpawned];

            // Kalikan step dengan indeks acak tersebut, BUKAN dengan m_undynesSpawned
            float angleDeg = m_undyneParams.arcMinAngle + (randomPositionIndex * step);
            float rad = DirectX::XMConvertToRadians(angleDeg);

            // Hitung koordinat dari titik pusat busur
            DirectX::XMFLOAT3 spawnPos = {
                m_undyneParams.arcCenterX + cosf(rad) * m_undyneParams.arcRadius,
                0.0f,
                m_undyneParams.arcCenterZ + sinf(rad) * m_undyneParams.arcRadius
            };

            // Setup Tombak
            UndyneSpearWindow spear;
            spear.bullet = std::make_unique<Bullet>();
            spear.bullet->SetActive(true);
            spear.bullet->ApplyMovement(spawnPos, { 0, 0, 0 });
            spear.bullet->SetDamage(m_undyneParams.damage);
            spear.bullet->SetRadius(0.8f);

            spear.windowName = "Spear_" + std::to_string(SDL_GetTicks()) + "_" + std::to_string(m_undynesSpawned);
            TrackedWindowConfig config;
            config.name = spear.windowName;
            config.title = "Spear.exe";
            config.width = 100;
            config.height = 100;
            config.role = WindowRole::TRACKED_ENTITY;
            config.priority = 10;

            Bullet* rawBullet = spear.bullet.get();
            boss->GetWindowSystem()->AddTrackedWindow(config, [rawBullet]() {
                return rawBullet->GetMovement()->GetPosition();
                });

            m_undyneSpears.push_back(std::move(spear));

            if (++m_undynesSpawned >= m_undyneParams.count) {
                m_isSpawningUndynes = false;
            }
        }
    }

    // =========================================================
    // 2. State Machine per Spear (Tanpa Laser)
    // =========================================================
    for (auto& spear : m_undyneSpears) {
        if (spear.isPreparedForDestroy) continue;

        spear.timer += dt;
        DirectX::XMFLOAT3 bPos = spear.bullet->GetMovement()->GetPosition();

        // Selama masih membidik (State 0), terus perbarui arah menghadap player
        if (spear.state == 0 && m_aiTarget) {
            DirectX::XMFLOAT3 pPos = m_aiTarget->GetPosition();
            float dx = pPos.x - bPos.x;
            float dz = pPos.z - bPos.z;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist > 0.001f) {
                spear.lockDir = { dx / dist, 0.0f, dz / dist };
                spear.bullet->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(atan2f(dx, dz)));
            }
        }

        // STATE 0: Membidik & Menunggu
        if (spear.state == 0) {
            if (spear.timer >= m_undyneParams.hoverDuration) {
                spear.state = 2; // Langsung beralih ke State Meluncur (Skip Laser)
                spear.timer = 0.0f;
                spear.currentSpeed = m_undyneParams.startSpeed;
            }
        }
        // STATE 2: Meluncur! 
        else if (spear.state == 2) {
            spear.currentSpeed += m_undyneParams.acceleration * dt;
            if (spear.currentSpeed > m_undyneParams.maxSpeed) {
                spear.currentSpeed = m_undyneParams.maxSpeed;
            }

            spear.bullet->ApplyMovement(bPos, {
                spear.lockDir.x * spear.currentSpeed,
                0.0f,
                spear.lockDir.z * spear.currentSpeed
                });
            spear.bullet->Update(dt, nullptr);

            if (bPos.x < -40.0f || bPos.x > 40.0f || bPos.z < -40.0f || bPos.z > 40.0f) {
                spear.isPreparedForDestroy = true;
                spear.bullet->SetActive(false);
                auto extracted = boss->GetWindowSystem()->ExtractForPool(spear.windowName);
                if (extracted && extracted->window) {
                    WindowManager::Instance().DestroyWindow(extracted->window);
                }
            }
        }
    }
}

void NaviPhaseWindowkill::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera || !m_wingSprite || !boss) return;

    bool isFXCam = (currentCamera == m_fxCamera.get());
    bool isDialogueCam = m_dialogueCamera && (currentCamera == m_dialogueCamera.get());
    bool isMainCam = !isFXCam && !isDialogueCam;
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
    bool isDeathMainCam = m_isDying && isMainCam;

    // =========================================================
    // RENDER DIALOGUE KE TRACKING WINDOW-NYA SENDIRI
    // Engine memanggil Render() untuk setiap kamera aktif.
    // Saat giliran kamera dialogue, hanya teks yang dirender —
    // window ini transparan & no background, isi cuma font.
    // =========================================================
    if (isDialogueCam) {
        if (m_isDialogueActive && m_dialogueBox && m_dialogueBox->IsActive()) {
            m_dialogueBox->RenderToWindow(context, m_dialogueWindowW, m_dialogueWindowH);
        }
        return; // Tidak ada hal lain yang perlu dirender di window ini
    }

    // =========================================================
    // 1. RENDER SAYAP
    // Normalnya hanya di FX cam, tapi saat mati juga di main cam
    // =========================================================
    if (isFXCam || isDeathMainCam) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        DirectX::XMFLOAT3 bossPos = boss->GetPosition();

        float leftWingX = bossPos.x - m_wingXOffset;
        for (const auto& node : m_leftWingData) {
            if (node.animScale <= 0.0f) continue;
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            batchData.push_back({
                leftWingX + node.localOffset.x, bossPos.y - 0.1f, bossPos.z + m_wingZOffset + node.localOffset.y,
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }
        float rightWingX = bossPos.x + m_wingXOffset;
        for (const auto& node : m_rightWingData) {
            if (node.animScale <= 0.0f) continue;
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            batchData.push_back({
                rightWingX + node.localOffset.x, bossPos.y - 0.1f, bossPos.z + m_wingZOffset + node.localOffset.y,
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }

        if (!batchData.empty()) {
            m_wingSprite->Render3DBatch(context, currentCamera, batchData);
        }
    }

    // =========================================================
        // [UPDATE] RENDER OVERDRIVE SPRITE (DECAL LANTAI)
        // =========================================================
        // Hanya render jika alpha > 0 untuk menghemat draw call, dan pastikan kandang masih ada
    if (m_aiTarget && m_aiTarget->IsPowerUncapped() && m_isPlayerCaged && m_overdriveAlpha > 0.0f && m_overdriveSprite) {
        std::vector<Sprite::Sprite3DBatchData> overdriveBatch;

        float baseWidth = 592.0f;
        float baseHeight = 193.0f;
        float finalWidth = baseWidth * m_overdriveSpriteScale;
        float finalHeight = baseHeight * m_overdriveSpriteScale;

        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, -7.5f };
        float scale = 0.4f;

        overdriveBatch.push_back({
            pos.x, pos.y, pos.z,
            finalWidth * scale, finalHeight * scale,
            0.0f, 0.0f, 0.0f, 0.0f,
            DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, m_overdriveAlpha * 0.5f// <-- Terapkan m_overdriveAlpha di channel Alpha (RGBA)
            });

        m_overdriveSprite->Render3DBatch(context, currentCamera, overdriveBatch);
    }

    for (auto& bwb : m_bouncingBullets) {
        if (bwb.bullet && bwb.bullet->IsActive()) {
            auto modelRenderer = Graphics::Instance().GetModelRenderer();

            // Gambar bola 3D peluru dengan warna Oranye Terang
            modelRenderer->Draw(ShaderId::Phong, bwb.bullet->GetModel(), { 1.0f, 0.4f, 0.0f, 1.0f });
        }
    }

    for (auto& bw : m_boomerangs) {
        if (bw.bullet && bw.bullet->IsActive()) {
            auto modelRenderer = Graphics::Instance().GetModelRenderer();
            // Warna Ungu Gelap untuk membedakan Boomerang
            modelRenderer->Draw(ShaderId::Phong, bw.bullet->GetModel(), { 0.6f, 0.0f, 0.8f, 1.0f });
        }
    }

    // =========================================================
        // 2. RENDER SEMUA ORBITAL BLASTER AKTIF
        // =========================================================
    for (auto& b : m_blasters) {
        if (!b->active) continue;

        // A. GAMBAR KEPALA MERIAM (Model 3D)
        if (m_placeholderModel && !m_placeholderModel->GetNodes().empty()) {
            auto modelRenderer = Graphics::Instance().GetModelRenderer();
            auto& rootNode = m_placeholderModel->GetNodes().at(0);
            DirectX::XMFLOAT4X4 identity;
            DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

            rootNode.position = b->pos; // Gunakan b->pos
            float cScale = m_blasterParams.cannonVisualScale;
            rootNode.scale = { cScale, cScale, cScale };
            m_placeholderModel->UpdateTransform(identity);
            modelRenderer->Draw(ShaderId::Phong, m_placeholderModel, { 0.2f, 0.5f, 0.5f, 1.0f });
        }

        // B. GAMBAR LASER BEAM (Solid 2D Primitive)
        if (b->beamScaleX > 0.0f && m_solidRenderer) {
            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            float startX, startY, endY;

            boss->GetWindowSystem()->WorldToScreenPos(b->pos, startX, startY);
            DirectX::XMFLOAT3 endPos = b->pos;
            endPos.z -= b->beamCurrentLength;
            float dummyX;
            boss->GetWindowSystem()->WorldToScreenPos(endPos, dummyX, endY);

            DirectX::XMFLOAT3 camPos = currentCamera->GetPosition();
            startX -= (camPos.x * p2u);
            startY += (camPos.z * p2u);
            endY += (camPos.z * p2u);

            for (auto& tw : boss->GetWindowSystem()->GetWindows()) {
                if (tw->camera.get() == currentCamera) {
                    startX -= (float)tw->state.actualX;
                    startY -= (float)tw->state.actualY;
                    endY -= (float)tw->state.actualY;
                    break;
                }
            }

            float pixelWidth = b->beamScaleX * p2u; // Gunakan b->beamScaleX
            float pixelHeight = endY - startY;

            DirectX::XMFLOAT4 color = (b->state == 2) ?
                DirectX::XMFLOAT4{ 1,0,0,0.5f } : DirectX::XMFLOAT4{ 0,1,1,0.9f };

            m_solidRenderer->Rect(startX, startY, pixelWidth, pixelHeight, pixelWidth * 0.5f, 0.0f, 0.0f, color.x, color.y, color.z, color.w);
            m_solidRenderer->Render(context);
        }
    }

}


void NaviPhaseWindowkill::TriggerBouncingWindows(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // Hidupkan sistem spawner beruntun!
    m_isSpawningBouncing = true;
    m_bouncingSpawned = 0;
    m_bouncingSpawnTimer = m_bouncingParams.spawnDelay; // Paksa spawn peluru pertama instan
}

void NaviPhaseWindowkill::UpdateAI(float dt, NaviBoss* boss) {
    if (!boss || !m_aiTarget || !m_aiEnabled || m_isPlayerCaged || m_bossHP <= 0) return;

    std::mt19937 gen(std::random_device{}());

    // [AGRESIF] Hanya tunggu sampai proses "Spawning" selesai. 
    // Jangan tunggu sampai peluru hilang dari layar! Biarkan serangan overlap.
    bool isBusy = m_isSpawningBouncing || m_isSpawningBoomerangs ||
        m_isSpawningBlasters || m_isSpawningTargetedBlasters || m_isSpawningUndynes;

    if (!isBusy) {
        if (m_aiGlobalCooldown > 0.0f) {
            m_aiGlobalCooldown -= dt;
        }
        else {
            m_cdBouncing -= dt;
            m_cdBoomerang -= dt;
            m_cdOrbitalBlaster -= dt;
            m_cdTargetedBlaster -= dt;
            m_cdUndyne -= dt;
        }
    }

    if (isBusy || m_aiGlobalCooldown > 0.0f) return;

    // [AGRESIF] Cooldown antar serangan dipotong hampir 50%
    if (m_cdUndyne <= 0.0f) {
        TriggerUndyneSpear(boss);
        m_cdUndyne = std::uniform_real_distribution<float>(7.0f, 10.0f)(gen); // Awalnya 12-18
        m_aiGlobalCooldown = 0.4f; // Awalnya 0.8f
    }
    else if (m_cdTargetedBlaster <= 0.0f) {
        TriggerTargetedBlaster(boss);
        m_cdTargetedBlaster = std::uniform_real_distribution<float>(5.0f, 8.0f)(gen); // Awalnya 9-14
        m_aiGlobalCooldown = 0.3f; // Awalnya 0.6f
    }
    else if (m_cdOrbitalBlaster <= 0.0f) {
        TriggerOrbitalBlaster(boss);
        m_cdOrbitalBlaster = std::uniform_real_distribution<float>(4.0f, 7.0f)(gen); // Awalnya 7-12
        m_aiGlobalCooldown = 0.3f; // Awalnya 0.6f
    }
    else if (m_cdBoomerang <= 0.0f) {
        TriggerBoomerang(boss);
        m_cdBoomerang = std::uniform_real_distribution<float>(3.0f, 5.0f)(gen); // Awalnya 4-8
        m_aiGlobalCooldown = 0.2f; // Awalnya 0.4f
    }
    else if (m_cdBouncing <= 0.0f) {
        TriggerBouncingWindows(boss);
        m_cdBouncing = std::uniform_real_distribution<float>(1.5f, 3.0f)(gen); // Awalnya 2-5
        m_aiGlobalCooldown = 0.15f; // Awalnya 0.3f
    }
}
std::vector<Bullet*> NaviPhaseWindowkill::GetProjectiles() {
    std::vector<Bullet*> activeBullets;
    for (auto& bwb : m_bouncingBullets) {
        if (bwb.bullet && bwb.bullet->IsActive()) {
            activeBullets.push_back(bwb.bullet.get());
        }
    }
    // [NEW] Daftarkan Boomerang ke wasit CollisionManager!
    for (auto& bw : m_boomerangs) {
        if (bw.bullet && bw.bullet->IsActive()) {
            activeBullets.push_back(bw.bullet.get());
        }
    }

    for (auto& spear : m_undyneSpears) {
        if (!spear.isPreparedForDestroy && spear.bullet && spear.bullet->IsActive() && spear.state == 2) {
            // Hanya berbahaya saat State = 2 (Meluncur)
            activeBullets.push_back(spear.bullet.get());
        }
    }
    return activeBullets;
}

void NaviPhaseWindowkill::TriggerOrbitalBlaster(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    if (!m_placeholderModel) {
        auto device = Graphics::Instance().GetDevice();
        m_placeholderModel = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");
    }

    // Aktifkan sistem spawner!
    m_isSpawningBlasters = true;
    m_blastersSpawned = 0;
    m_blasterSpawnTimer = m_blasterParams.spawnDelay; // Paksa agar langsung spawn yang pertama
}

void NaviPhaseWindowkill::TriggerBoomerang(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // Aktifkan sistem spawner beruntun!
    m_isSpawningBoomerangs = true;
    m_boomerangsSpawned = 0;
    m_boomerangSpawnTimer = m_boomerangParams.spawnDelay; // Paksa spawn yang pertama
}

void NaviPhaseWindowkill::TriggerUndyneSpear(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    m_isSpawningUndynes = true;
    m_undynesSpawned = 0;
    m_undyneSpawnTimer = 0.0f;

    // =========================================================
    // [BARU] Siapkan urutan posisi spawn yang diacak
    // =========================================================
    m_undyneSpawnIndices.clear();
    for (int i = 0; i < m_undyneParams.count; ++i) {
        m_undyneSpawnIndices.push_back(i); // Isi dengan: 0, 1, 2, 3...
    }

    // Acak urutan angkanya (misal menjadi: 3, 0, 5, 1...)
    static std::mt19937 gen(std::random_device{}());
    std::shuffle(m_undyneSpawnIndices.begin(), m_undyneSpawnIndices.end(), gen);
}

void NaviPhaseWindowkill::TriggerTargetedBlaster(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    if (!m_placeholderModel) {
        auto device = Graphics::Instance().GetDevice();
        m_placeholderModel = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");
    }

    // Aktifkan sistem spawner Targeted!
    m_isSpawningTargetedBlasters = true;
    m_targetedBlastersSpawned = 0;
    m_targetedBlasterSpawnTimer = m_targetedBlasterParams.spawnDelay;
}

void NaviPhaseWindowkill::DamageCage(int dmg) {
    if (!m_isPlayerCaged) return;

    bool isFirstHit = !m_cageFirstHitTriggered;
    if (isFirstHit) {
        m_cageFirstHitTriggered = true;
        TriggerCageFirstHitDialogue(m_bossRef);
    }

    m_cageHP -= dmg;
    if (m_cageHP < 0) m_cageHP = 0;

    // =========================================================
    // [FIX] KEMBALIKAN EFEK HIT YANG HILANG
    // =========================================================
    m_cageShakeTimer = m_cageShakeDuration;

    // [PENTING] Ganti tulisan "SE_Hit.wav" di bawah ini dengan 
    // nama file suara aslimu yang benar jika kamu menggunakan nama lain!
    AudioManager::Instance().PlaySFX("Data/Sound/SE_Hit.wav", 0.2f);

    if (m_bossRef && m_bossRef->GetWindowSystem()) {
        if (auto* cageWindow = m_bossRef->GetWindowSystem()->GetTrackedWindow(m_cageWindowName)) {
            if (cageWindow->window) {
                float hpRatio = (m_cageMaxHP > 0) ? ((float)m_cageHP / (float)m_cageMaxHP) : 0.0f;
                cageWindow->window->SetBackgroundAlpha(max(0.15f, hpRatio));
            }
        }
    }

    // =========================================================
    // PICU PLAYER OVERDRIVE / UNCAPPED (HP <= 50%)
    // =========================================================
    if (m_cageHP <= 300 && m_aiTarget) {
        if (!m_aiTarget->IsPowerUncapped()) {
            m_aiTarget->ReleasePowerCap();
            m_aiTarget->SetShootDelay(0.5f);

            // Beri efek getaran & suara dramatis saat player "Super Saiyan"
            CameraController::Instance().AddTrauma(0.5f);
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Laser_Charge.wav", 0.3f);
        }
    }

    // =========================================================
    // JIKA KANDANG HANCUR (HP <= 0)
    // =========================================================
    if (m_cageHP <= 0) {
        m_isPlayerCaged = false;
        m_aiEnabled = true;
        m_aiGlobalCooldown = 1.0f;

        CameraController::Instance().AddTrauma(0.6f);
        AudioManager::Instance().PlaySFX("Data/Sound/SE_GlassShatter.wav", 0.4f);

        if (m_bossRef && m_bossRef->GetWindowSystem()) {
            auto extracted = m_bossRef->GetWindowSystem()->ExtractForPool(m_cageWindowName);
            if (extracted && extracted->window) {
                WindowManager::Instance().DestroyWindow(extracted->window);
            }
        }
    }
}

void NaviPhaseWindowkill::TriggerCageFirstHitDialogue(NaviBoss* boss)
{
    // Dialog tidak dibutuhkan — fungsi dikosongkan dengan sengaja.
}

void NaviPhaseWindowkill::TakeDamage(int damage, DirectX::XMFLOAT3 hitPos) {
    if (m_bossHP <= 0) return;

    m_bossHP = max(0, m_bossHP - damage);
    m_hitFlashTimer = 0.05f;

    CameraController::Instance().AddTrauma(0.3f);
    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Hit.wav", 0.1f);
    EffectManager::Instance().Play("Data/Effect/VFX_Boss_Hit.efk", hitPos, 0.3f);
}