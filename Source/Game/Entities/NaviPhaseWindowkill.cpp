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
#include "EffectManager.h"

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

    boss->SetPosition({ 0.0f, 0.0f, 7.0f });
    boss->GetFaceParams().gridResolution = 16;
    boss->InitializeFaceGrid(Graphics::Instance().GetDevice());

    EffectManager::Instance().PreloadEffect(m_blasterParams.chargeEffectPath);
    EffectManager::Instance().PreloadEffect(m_blasterParams.fireEffectPath);
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
    for (auto& b : m_blasters) {
        EffectManager::Instance().Stop(b->chargeEffectHandle);
        if (boss && boss->GetWindowSystem()) {
            boss->GetWindowSystem()->RemoveTrackedWindow(b->beamWindowName);
            boss->GetWindowSystem()->RemoveTrackedWindow(b->windowName);
        }
    }
    m_blasters.clear();

    for (auto& bw : m_boomerangs) {
        if (boss && boss->GetWindowSystem()) boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
    }
    m_boomerangs.clear();
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

        // [PRO APPROACH] Hitung posisi efek satu kali saja untuk frame ini
        DirectX::XMFLOAT3 vfxPos = {
            b->pos.x + m_blasterParams.effectOffset.x,
            b->pos.y + m_blasterParams.effectOffset.y,
            b->pos.z + m_blasterParams.effectOffset.z
        };

        if (b->state == 1) { // DROP IN
            b->pos.z += (b->targetPos.z - b->pos.z) * 12.0f * dt;

            if (b->timer >= m_blasterParams.dropInDuration) {
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

            if (b->timer >= m_blasterParams.chargeDelay) {
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

            if (b->timer >= m_blasterParams.fireDuration) {
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
}



void NaviPhaseWindowkill::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera || !m_wingSprite || !boss) return;

    bool isFXCam = (currentCamera == m_fxCamera.get());
    bool isMainCam = !isFXCam;
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();

    // 1. RENDER SAYAP (Sistem Sprite 3D)
    // Sayap adalah objek 3D, jadi otomatis muncul di semua window portal!
    if (isFXCam) {
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