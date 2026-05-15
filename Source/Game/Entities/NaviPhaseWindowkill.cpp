#include "NaviPhaseWindowkill.h"
#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include <algorithm>
#include <random>
#include <SceneBoss.h>
#include "WindowManager.h" 
#include <SDL3/SDL.h>
#include <System/AudioManager.h>

using namespace DirectX;

NaviPhaseWindowkill::NaviPhaseWindowkill() {}

// =========================================================
// [MAGIC] MEMORY MANAGEMENT & SPAWNING
// =========================================================
void NaviPhaseWindowkill::Enter(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

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

    // Reset timer dan ciptakan array
    m_glitchTimer = 0.0f;
    m_wingState = WingState::Expanding;
    m_wingStateTimer = 0.0f;
    GenerateButterflyWings();

    boss->SetPosition({ 0.0f, 0.0f, 0.0f });
}

void NaviPhaseWindowkill::Exit(NaviBoss* boss) {
    // BERSIH-BERSIH TOTAL SAAT FASE SELESAI!
    if (boss && boss->GetWindowSystem()) {
        boss->GetWindowSystem()->RemoveTrackedWindow("navi_fx");
    }
    m_fxWindow = nullptr;
    m_fxCamera.reset();
    m_wingSprite.reset(); // Bebaskan tekstur dari VRAM
    m_leftWingData.clear();
    m_rightWingData.clear();

    for (auto& bwb : m_bouncingBullets) {
        if (boss && boss->GetWindowSystem()) boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
    }
    m_bouncingBullets.clear();

    // =========================================================
        // [FIX] BERSIHKAN JUGA WINDOW MERIAM JIKA SEDANG AKTIF!
        // =========================================================
    if (m_testBlaster.active) {
        if (boss && boss->GetWindowSystem()) {
            // Hapus kedua jendela sekaligus
            boss->GetWindowSystem()->RemoveTrackedWindow(m_testBlaster.beamWindowName);
            boss->GetWindowSystem()->RemoveTrackedWindow(m_testBlaster.windowName);
        }
        m_testBlaster.active = false;
    }
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
    }

    // =========================================================
    // [NEW] UPDATE ORBITAL BLASTER (SANS LASER)
    // =========================================================
    if (m_testBlaster.active) {
        m_testBlaster.timer += dt;

        // STATE 1: DROP IN (Jatuh ke posisi tembak)
        if (m_testBlaster.state == 1) {
            // Lerp super mulus dengan kecepatan tinggi (Ease Out)
            m_testBlaster.pos.z += (m_testBlaster.targetPos.z - m_testBlaster.pos.z) * 12.0f * dt;

            if (m_testBlaster.timer >= 0.4f) {
                m_testBlaster.state = 2; // Masuk ke mode bidik
                m_testBlaster.timer = 0.0f;
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Charge.wav", 0.5f); // Opsional
            }
        }
        // STATE 2: CHARGING / TELEGRAPH (Garis Peringatan & Window Sliding)
        else if (m_testBlaster.state == 2) {
            m_testBlaster.pos.x = ((rand() % 100) / 100.0f - 0.5f) * 0.1f; // Getaran kepala
            m_testBlaster.beamScaleX = 0.2f; // Tipis (Peringatan)

            // Laser memanjang ke bawah dengan cepat
            float targetLength = m_blasterParams.beamMaxLength;
            m_testBlaster.beamCurrentLength += (targetLength - m_testBlaster.beamCurrentLength) * m_blasterParams.beamSlideSpeed * dt;

            // [HAPUS] Logika ws->AddTrackedWindow(beamCfg) sudah dihapus dari sini!

            if (m_testBlaster.timer >= 0.6f) {
                m_testBlaster.state = 3;
                m_testBlaster.timer = 0.0f;
                CameraController::Instance().AddTrauma(0.6f);
            }
        }

        // STATE 3: FIRING (Laser Raksasa!)
        else if (m_testBlaster.state == 3) {
            m_testBlaster.pos.x = 0.0f;

            // [FIX] Gunakan beamVisualWidth sebagai batas maksimal lebar laser!
            m_testBlaster.beamScaleX += (m_blasterParams.beamVisualWidth - m_testBlaster.beamScaleX) * m_blasterParams.beamGrowSpeed * dt;
            CameraController::Instance().AddTrauma(0.1f);

            if (m_testBlaster.timer >= 0.8f) { // Tembak selama 0.8 detik
                m_testBlaster.state = 4;
                m_testBlaster.timer = 0.0f;

                m_testBlaster.pos.z = -10000.0f;

                // =========================================================
                // [FIX] MATIKAN WINDOW LASER SECARA INSTAN DI SINI!
                // Ini mencegah window merosot saat meriam mundur di State 4
                // =========================================================
                if (boss && boss->GetWindowSystem()) {
                    boss->GetWindowSystem()->RemoveTrackedWindow(m_testBlaster.beamWindowName);
                }
            }
        }
        // STATE 4: RETREAT (Hilang)
        else if (m_testBlaster.state == 4) {
            m_testBlaster.beamScaleX -= dt * 25.0f; // Laser 2D menciut super cepat
            if (m_testBlaster.beamScaleX < 0.0f) m_testBlaster.beamScaleX = 0.0f;

            m_testBlaster.pos.z += 40.0f * dt; // Kepala meriam terbang ke atas

            if (m_testBlaster.timer >= 0.3f) {
                m_testBlaster.active = false; // Siklus selesai

                // Hanya hapus Cannon di sini (Laser sudah dihapus di atas)
                if (boss && boss->GetWindowSystem()) {
                    boss->GetWindowSystem()->RemoveTrackedWindow(m_testBlaster.windowName);
                }
            }
        }
    }
}

void NaviPhaseWindowkill::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera || !m_wingSprite || !boss) return;

    bool isFXCam = (currentCamera == m_fxCamera.get());
    bool isMainCam = !isFXCam;
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();

    // 1. RENDER SAYAP (Sistem Sprite 3D)
    // Sayap adalah objek 3D, jadi otomatis muncul di semua window portal!
    if (isFXCam || isMainCam) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        DirectX::XMFLOAT3 bossPos = boss->GetPosition();

        // (Logika Batching Sayap tetap sama...)
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
        // (Ulangi untuk sayap kanan...)
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

    for (auto& bwb : m_bouncingBullets) {
        if (bwb.bullet && bwb.bullet->IsActive()) {
            auto modelRenderer = Graphics::Instance().GetModelRenderer();

            // Gambar bola 3D peluru dengan warna Oranye Terang
            modelRenderer->Draw(ShaderId::Phong, bwb.bullet->GetModel(), { 1.0f, 0.4f, 0.0f, 1.0f });
        }
    }

    // =========================================================
    // 2. RENDER ORBITAL BLASTER (CANNON & LASER)
    // [FIX] Kita hapus isMainCam agar muncul di SEMUA window!
    // =========================================================
    if (m_testBlaster.active) {

        // A. GAMBAR KEPALA MERIAM (Model 3D)
        if (m_placeholderModel && !m_placeholderModel->GetNodes().empty()) {
            auto modelRenderer = Graphics::Instance().GetModelRenderer();
            auto& rootNode = m_placeholderModel->GetNodes().at(0);
            DirectX::XMFLOAT4X4 identity;
            DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

            rootNode.position = m_testBlaster.pos;
            // [FIX] Skala 3D meriam menggunakan cannonVisualScale
            float cScale = m_blasterParams.cannonVisualScale;
            rootNode.scale = { cScale, cScale, cScale };
            m_placeholderModel->UpdateTransform(identity);
            modelRenderer->Draw(ShaderId::Phong, m_placeholderModel, { 0.2f, 0.5f, 0.5f, 1.0f });
        }

        // B. GAMBAR LASER BEAM (Solid 2D Primitive)
        // Agar laser 2D muncul di portal, kita harus geser koordinatnya sesuai posisi Window OS!
        if (m_testBlaster.beamScaleX > 0.0f && m_solidRenderer) {
            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            float startX, startY, endY;

            // Ambil posisi layar absolut (Desktop)
            boss->GetWindowSystem()->WorldToScreenPos(m_testBlaster.pos, startX, startY);
            DirectX::XMFLOAT3 endPos = m_testBlaster.pos;
            endPos.z -= m_testBlaster.beamCurrentLength;
            float dummyX;
            boss->GetWindowSystem()->WorldToScreenPos(endPos, dummyX, endY);

            // Kompensasi Kamera (Shake & Position)
            DirectX::XMFLOAT3 camPos = currentCamera->GetPosition();
            startX -= (camPos.x * p2u);
            startY += (camPos.z * p2u);
            endY += (camPos.z * p2u);

            // ---------------------------------------------------------
            // [PRO-LOGIC] OFFSET BERDASARKAN POSISI WINDOW OS
            // ---------------------------------------------------------
            // Cari tahu jendela mana yang sedang dirender saat ini
            for (auto& tw : boss->GetWindowSystem()->GetWindows()) {
                if (tw->camera.get() == currentCamera) {
                    // Geser koordinat laser agar relatif terhadap pojok kiri atas jendela ini!
                    startX -= (float)tw->state.actualX;
                    startY -= (float)tw->state.actualY;
                    endY -= (float)tw->state.actualY;
                    break;
                }
            }

            float pixelWidth = m_testBlaster.beamScaleX * p2u;
            float pixelHeight = endY - startY;

            DirectX::XMFLOAT4 color = (m_testBlaster.state == 2) ?
                DirectX::XMFLOAT4{ 1,0,0,0.5f } : DirectX::XMFLOAT4{ 0,1,1,0.9f };

            m_solidRenderer->Rect(startX, startY, pixelWidth, pixelHeight, pixelWidth * 0.5f, 0.0f, 0.0f, color.x, color.y, color.z, color.w);
            m_solidRenderer->Render(context);
        }
    }

    // HITBOX BOUNCING BULLET HILANG DARI SINI? 
    // Jangan khawatir! Hitbox sekarang sudah dirender secara terpusat di SceneBoss.cpp 
    // agar sinkron dengan checkbox "Show Hitboxes" dan muncul di semua jendela secara otomatis.
}


void NaviPhaseWindowkill::TriggerBouncingWindows(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    for (int i = 0; i < m_bouncingParams.spawnCount; i++) {
        BouncingWindowBullet bwb;
        bwb.bullet = std::make_unique<Bullet>();

        // 1. SET HITBOX (FISIK)
        // Menggunakan hitboxRadius untuk perhitungan tabrakan
        bwb.bullet->SetRadius(m_bouncingParams.hitboxRadius);

        // 2. SET MODEL SCALE (VISUAL)
        // Menggunakan visualScale untuk ukuran bola di dalam game
        float vs = m_bouncingParams.visualScale;
        bwb.bullet->scale = { vs, vs, vs };

        bwb.maxBounces = m_bouncingParams.maxBounces;

        // Logika Gerak
        float angle = (float)(rand() % 360);
        DirectX::XMFLOAT3 dir = { cosf(angle), 0, sinf(angle) };
        bwb.bullet->Fire(boss->GetPosition(), dir, m_bouncingParams.speed);

        // 3. SET WINDOW SIZE (MANUAL & INDEPENDEN)
        bwb.windowName = "bouncing_win_" + std::to_string(rand() % 10000);

        TrackedWindowConfig cfg;
        cfg.name = bwb.windowName;
        cfg.title = "PROJECTILE";
        cfg.width = (int)m_bouncingParams.windowWidth;
        cfg.height = (int)m_bouncingParams.windowHeight;
        cfg.isTransparent = false;

        // =========================================================
        // [FIX MUTLAK 1] TAMBAHKAN LAMBDA UNTUK GET TARGET SIZE
        // =========================================================
        boss->GetWindowSystem()->AddTrackedWindow(cfg,
            [ptr = bwb.bullet.get()]() { return ptr->GetPosition(); },
            [this]() { return DirectX::XMFLOAT2(m_bouncingParams.windowWidth, m_bouncingParams.windowHeight); }
        );

        m_bouncingBullets.push_back(std::move(bwb));
    }

    CameraController::Instance().AddTrauma(0.2f);
}

std::vector<Bullet*> NaviPhaseWindowkill::GetProjectiles() {
    std::vector<Bullet*> activeBullets;
    for (auto& bwb : m_bouncingBullets) {
        if (bwb.bullet && bwb.bullet->IsActive()) {
            activeBullets.push_back(bwb.bullet.get());
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

    m_testBlaster.active = true;
    m_testBlaster.state = 1;
    m_testBlaster.timer = 0.0f;
    m_testBlaster.pos = { 0.0f, 1.0f, 25.0f };
    m_testBlaster.targetPos = { 0.0f, 1.0f, 10.0f };
    m_testBlaster.beamScaleX = 0.0f;
    m_testBlaster.beamCurrentLength = 0.0f; // [FIX] Pastikan mulai dari 0

    // 1. SPAWN LASER WINDOW DULUAN 
    m_testBlaster.beamWindowName = "blaster_beam_window";
    TrackedWindowConfig beamCfg;
    beamCfg.name = m_testBlaster.beamWindowName;
    beamCfg.title = "!!! BEAM REACHING !!!";
    beamCfg.isTransparent = false;
    beamCfg.priority = 1;

    boss->GetWindowSystem()->AddTrackedWindow(beamCfg,
        [this]() {
            if (m_testBlaster.state < 2) {
                DirectX::XMFLOAT3 hiddenPos = m_testBlaster.pos;
                hiddenPos.z = -10000.0f;
                return hiddenPos;
            }
            return m_testBlaster.pos;
        },
        [this, boss]() {
            if (m_testBlaster.state < 2) return DirectX::XMFLOAT2(1.0f, 1.0f);

            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            // [FIX] Lebar window laser menyesuaikan visual laser + margin ekstra agar tidak kepotong border OS
            float w = max(50.0f, (m_testBlaster.beamScaleX + 1.5f) * p2u);
            float h = max(1.0f, m_testBlaster.beamCurrentLength * p2u);
            return DirectX::XMFLOAT2(w, h);
        }
    );

    // 2. SPAWN CANNON WINDOW KEDUA
    m_testBlaster.windowName = "blaster_cannon_window";
    TrackedWindowConfig cfg;
    cfg.name = m_testBlaster.windowName;
    cfg.title = "DANGER: ORBITAL CANNON";
    // [FIX] Gunakan parameter khusus window untuk meriam
    cfg.width = (int)m_blasterParams.cannonWindowSize;
    cfg.height = (int)m_blasterParams.cannonWindowSize;
    cfg.role = WindowRole::TRACKED_ENTITY;
    cfg.isTransparent = false;
    cfg.priority = 10;

    boss->GetWindowSystem()->AddTrackedWindow(cfg,
        [this]() { return m_testBlaster.pos; },
        [this]() {
            // [FIX] Lambda ukuran window mengembalikan ukuran statis dari ImGui
            float size = m_blasterParams.cannonWindowSize;
            return DirectX::XMFLOAT2(size, size);
        }
    );
}