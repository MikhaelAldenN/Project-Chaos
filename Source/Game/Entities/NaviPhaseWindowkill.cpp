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

        // =========================================================
        // [FIX MUTLAK] PENGHAPUSAN JENDELA SAAT KENA PLAYER
        // Jika peluru dimatikan oleh CollisionManager, hapus windownya!
        // =========================================================
        if (!bwb.bullet->IsActive()) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
            it = m_bouncingBullets.erase(it); // Hapus dari memori vector
            continue; // Lanjut ke peluru berikutnya
        }

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
        CameraController::Instance().AddTrauma(0.2f);
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
        // STATE 2: CHARGING / TELEGRAPH (Garis Peringatan)
        else if (m_testBlaster.state == 2) {
            // Getarkan kepala meriam untuk memberi kesan "Overload"
            m_testBlaster.pos.x = ((rand() % 100) / 100.0f - 0.5f) * 0.4f;
            m_testBlaster.beamScaleX = 0.2f; // Lebar laser sangat tipis (Peringatan!)

            if (m_testBlaster.timer >= 0.6f) { // Jeda 0.6 detik agar player bisa bereaksi
                m_testBlaster.state = 3;
                m_testBlaster.timer = 0.0f;
                CameraController::Instance().AddTrauma(0.6f); // GETARAN MAUT SAAT MENEMBAK!
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Laser.wav", 1.0f); // Opsional
            }
        }
        // STATE 3: FIRING (Laser Raksasa!)
        else if (m_testBlaster.state == 3) {
            m_testBlaster.pos.x = 0.0f; // Kunci posisi X ke tengah lagi

            // Laser membesar tiba-tiba ke ukuran raksasa
            m_testBlaster.beamScaleX += (4.0f - m_testBlaster.beamScaleX) * 20.0f * dt;
            CameraController::Instance().AddTrauma(0.1f); // Getaran bergemuruh konstan

            if (m_testBlaster.timer >= 0.8f) { // Tembak selama 0.8 detik
                m_testBlaster.state = 4;
                m_testBlaster.timer = 0.0f;
            }
        }
        // STATE 4: RETREAT (Hilang)
        else if (m_testBlaster.state == 4) {
            m_testBlaster.beamScaleX -= dt * 25.0f; // Laser menciut super cepat
            if (m_testBlaster.beamScaleX < 0.0f) m_testBlaster.beamScaleX = 0.0f;

            m_testBlaster.pos.z += 40.0f * dt; // Kepala meriam terbang ke atas

            if (m_testBlaster.timer >= 0.3f) {
                m_testBlaster.active = false; // Siklus selesai
            }
        }
    }

}

void NaviPhaseWindowkill::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera || !m_wingSprite || !boss) return;

    bool isFXCam = (currentCamera == m_fxCamera.get());
    bool isMainCam = !isFXCam;

    if (isFXCam || isMainCam) {
        std::vector<Sprite::Sprite3DBatchData> batchData;

        // Ambil posisi global dari Boss
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

    if (isMainCam) {
        for (auto& bwb : m_bouncingBullets) {
            if (bwb.bullet->IsActive()) {
                Graphics::Instance().GetModelRenderer()->Draw(ShaderId::Phong, bwb.bullet->GetModel(), { 1.0f, 0.2f, 0.0f, 1.0f });
            }
        }
    }
    // =========================================================
        // [NEW] RENDER ORBITAL BLASTER & LASER BEAM
        // =========================================================
    if (m_testBlaster.active && m_placeholderModel && isMainCam) {
        auto renderer = Graphics::Instance().GetModelRenderer();

        DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f);

        // =========================================================
        // [FIX MUTLAK] SUNTIKKAN STEROID KE SKALA MODEL!
        // =========================================================
        // Kalikan skala kepala meriam hingga 6x lipat agar raksasa!
        float giantHeadScale = m_testBlaster.headScale * 6.0f;

        // 1. GAMBAR KEPALA MERIAM
        DirectX::XMMATRIX headS = DirectX::XMMatrixScaling(giantHeadScale, giantHeadScale, giantHeadScale);
        DirectX::XMMATRIX headT = DirectX::XMMatrixTranslation(m_testBlaster.pos.x, m_testBlaster.pos.y, m_testBlaster.pos.z);

        DirectX::XMFLOAT4X4 headMatrix;
        DirectX::XMStoreFloat4x4(&headMatrix, headS * rotMatrix * headT);

        // Warna abu-abu gelap agar terlihat seperti besi mesin
        renderer->Draw(ShaderId::Phong, m_placeholderModel, { 0.3f, 0.3f, 0.3f, 1.0f }, headMatrix);

        // 2. GAMBAR LASER BEAM
        if (m_testBlaster.beamScaleX > 0.0f) {
            float laserLength = 120.0f; // Panjangkan hingga 120 unit agar pasti menembus bawah layar!

            // Lebarkan tiang lasernya agar tidak terlihat seperti lidi
            float giantBeamWidth = m_testBlaster.beamScaleX * 6.0f;
            float giantBeamThick = 6.0f; // Sumbu Y (Ketebalan) agar tidak pipih

            DirectX::XMMATRIX beamS = DirectX::XMMatrixScaling(giantBeamWidth, giantBeamThick, laserLength);

            // Titik tengah bola ditarik ke bawah sebesar setengah panjangnya
            // Agar ujung atasnya menempel pas di kepala meriam!
            DirectX::XMMATRIX beamT = DirectX::XMMatrixTranslation(m_testBlaster.pos.x, m_testBlaster.pos.y, m_testBlaster.pos.z - (laserLength * 0.5f));

            DirectX::XMFLOAT4X4 beamMatrix;
            DirectX::XMStoreFloat4x4(&beamMatrix, beamS * rotMatrix * beamT);

            // Merah transparan saat ngisi (State 2), Putih menyala saat nembak (State 3/4)
            DirectX::XMFLOAT4 beamColor = (m_testBlaster.state == 2) ?
                DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 0.4f } :
                DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };

            // Render dengan Shader::Basic agar laser tidak memiliki bayangan (Full Glow!)
            renderer->Draw(ShaderId::Basic, m_placeholderModel, beamColor, beamMatrix);
        }

        // 3. GAMBAR COLLISION BOX (KOTAK MERAH UNTUK DEBUG)
        if (m_testBlaster.state == 3) {
            float laserLength = 120.0f;
            float beamWidth = m_testBlaster.beamScaleX * 6.0f;

            // Gunakan Skala Kotak (X = Lebar Laser, Y = Tipis, Z = Panjang Laser)
            DirectX::XMMATRIX debugS = DirectX::XMMatrixScaling(beamWidth, 0.1f, laserLength);

            // Posisikan tepat di lantai (Y = 0.05f agar tidak z-fighting dengan lantai)
            DirectX::XMMATRIX debugT = DirectX::XMMatrixTranslation(m_testBlaster.pos.x, 0.05f, m_testBlaster.pos.z - (laserLength * 0.5f));

            DirectX::XMFLOAT4X4 debugMatrix;
            DirectX::XMStoreFloat4x4(&debugMatrix, debugS * debugT);

            // Render Kotak Merah Transparan
            renderer->Draw(ShaderId::Basic, m_placeholderModel, { 1.0f, 0.0f, 0.0f, 0.4f }, debugMatrix);
        }
    }
}

void NaviPhaseWindowkill::TriggerBouncingWindows(NaviBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // =========================================================
    // [FIX MUTLAK] KOREOGRAFI TANPA RNG (DETERMINISTIC)
    // Peluru 0: Kiri Atas (-45 derajat), Kecepatan Normal
    // Peluru 1: Kanan Atas (45 derajat), Kecepatan Normal
    // Peluru 2: Kanan Atas Agak Lebar (65 derajat), Kecepatan Ekstra (Biar memisah!)
    // =========================================================
    float fixedAngles[3] = { -DirectX::XM_PIDIV4 - 0.15f, DirectX::XM_PIDIV4, DirectX::XM_PIDIV4 + 0.35f };
    float fixedSpeeds[3] = { 30.0f, 30.0f, 34.0f };

    DirectX::XMFLOAT3 startPos = boss->GetPosition();

    for (int i = 0; i < 3; ++i) {
        BouncingWindowBullet bwb;
        bwb.bullet = std::make_unique<Bullet>();
        bwb.bounceCount = 0;
        bwb.maxBounces = 5;

        bwb.bullet->SetRadius(1.6f);
        bwb.bullet->scale = { 3.5f, 3.5f, 3.5f };
        bwb.bullet->SetActive(true);
        bwb.bullet->SetHomingTarget(nullptr);
        bwb.bullet->SetBossTarget(nullptr);

        // Tidak ada lagi random, langsung ambil dari array!
        float finalAngle = fixedAngles[i];
        float finalSpeed = fixedSpeeds[i];

        DirectX::XMFLOAT3 dir = { sinf(finalAngle), 0.0f, cosf(finalAngle) };
        bwb.bullet->Fire(startPos, dir, finalSpeed);

        m_bounceCounter++;
        bwb.windowName = "bounce_win_" + std::to_string(m_bounceCounter);

        TrackedWindowConfig cfg;
        cfg.name = bwb.windowName;
        cfg.title = "DANGER!";
        cfg.width = 200;
        cfg.height = 200;
        cfg.role = WindowRole::TRACKED_ENTITY;
        cfg.isTransparent = false;
        cfg.priority = 2;

        Bullet* bPtr = bwb.bullet.get();
        boss->GetWindowSystem()->AddTrackedWindow(
            cfg,
            [bPtr]() { return bPtr->GetMovement()->GetPosition(); },
            []() { return DirectX::XMFLOAT2(200.0f, 200.0f); }
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

void NaviPhaseWindowkill::TriggerOrbitalBlaster() {
    // =========================================================
    // [FIX MUTLAK 1] LAZY-LOAD MODEL
    // Pastikan model 100% terisi saat tombol ditekan!
    // =========================================================
    if (!m_placeholderModel) {
        auto device = Graphics::Instance().GetDevice();
        m_placeholderModel = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");
    }

    m_testBlaster.active = true;
    m_testBlaster.state = 1;
    m_testBlaster.timer = 0.0f;

    // =========================================================
    // [FIX MUTLAK 2] TURUNKAN POSISI Z
    // Batas atas layar biasanya Z = 13.0f. 
    // Kita taruh target di Z = 10.0f agar kepala meriam terlihat jelas!
    // =========================================================
    m_testBlaster.pos = { 0.0f, 1.0f, 25.0f };       // Jatuh dari atas layar
    m_testBlaster.targetPos = { 0.0f, 1.0f, 10.0f }; // Mendarat DI DALAM layar
    m_testBlaster.beamScaleX = 0.0f;
}