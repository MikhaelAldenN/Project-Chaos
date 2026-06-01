#pragma execution_character_set("utf-8")
#include "AttackRain.h"
#include "NaviBoss.h"
#include "System/Graphics.h"
#include "System/AudioManager.h"
#include "CameraController.h"
#include "EffectManager.h"
#include "Camera.h"
#include <random>
#include <cmath>
#include <windows.h>
#include "WindowTrackingSystem.h"
#include "Player.h"

using namespace DirectX;

AttackRain::AttackRain(const RainParams& params, RainMode mode, bool isPositiveSide, float sweepDir, Player* target)
    : m_params(params), m_mode(mode), m_isPositiveSide(isPositiveSide), m_sweepDir(sweepDir), m_target(target)
{
    m_vfxHandles.assign(800, -1);
}

void AttackRain::StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) {
    m_pool = pool;
    m_state = 1;
    m_timer = 0.0f;

    auto device = Graphics::Instance().GetDevice();
    m_zonePrimitive = std::make_unique<Primitive>(device);

    // Place zone center based on mode
    switch (m_mode) {
    case RainMode::DualPillar:
        m_center = { -16.0f, 0.0f,  0.0f };
        m_center2 = { 16.0f, 0.0f,  0.0f };
        break;
    case RainMode::VerticalSweep:
        m_center = { m_isPositiveSide ? 12.5f : -12.5f, 0.0f, 0.0f };
        break;
    case RainMode::HorizontalSweep:
        m_center = { 0.0f, 0.0f, m_isPositiveSide ? 7.5f : -7.5f };
        break;
    }
}

void AttackRain::Update(float dt, NaviBoss* boss) {
    if (m_state == 0) return;

    m_timer += dt;

    // Continuous screen shake during active + dissipating rain
    if (m_state == 2 || m_state == 3)
        CameraController::Instance().AddTrauma(1.0f * dt);

    if (m_state == 1) { // Warning
        if (m_timer >= m_params.warningDuration) {
            m_state = 2;
            m_timer = 0.0f;
            CameraController::Instance().AddTrauma(0.5f);
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Rain_01.wav", 0.07f * m_params.sfxVolume);
            // Anggap SFX_LOOP bernilai sesuai define kamu
            m_sfxTimer = 999.0f;
        }
    }
    else if (m_state == 2) { // Active
        m_sfxTimer += dt;
        // Gunakan angka spesifik jika SFX_LOOP tidak terbaca, misal 0.5f
        if (m_sfxTimer >= 0.5f) {
            std::string rainSounds[] = {
                "Data/Sound/SE_Boss_Rain_01.wav",
                "Data/Sound/SE_Boss_Rain_02.wav",
                "Data/Sound/SE_Boss_Rain_03.wav"
            };
            for (int i = 0; i < 3; ++i) {
                float delay = 0.5f + ((rand() % 301) / 1000.0f);
                AudioManager::Instance().PlaySFXDelayed(
                    rainSounds[rand() % 3], 0.07f * m_params.sfxVolume, delay);
            }
            m_sfxTimer = 0.0f;
        }
        if (m_timer >= m_params.activeDuration) {
            m_state = 3;
            m_timer = 0.0f;
        }
    }
    else if (m_state == 3) { // Dissipating
        m_sfxTimer = 0.0f;
        // Hold long enough for the render loop to finish the last visible drops
        if (m_timer >= 4.0f) {
            m_state = 0;
            ClearVFX();
            return; // PENTING: Keluar agar tidak lanjut cek collision di bawah
        }
    }

    // =========================================================
    // [FIX 3] DETEKSI HITBOX (COLLISION) PROSEDURAL DI UPDATE
    // =========================================================
    if ((m_state == 2 || m_state == 3) && m_target && m_target->GetHP() > 0) {

        float actualW = GetActualWidth();
        float actualD = GetActualDepth();
        float halfW = actualW * 0.5f;
        float halfD = actualD * 0.5f;

        // Gunakan seed yang SAMA PERSIS dengan Render agar posisi peluru identik
        std::mt19937 gen(1337);
        std::uniform_real_distribution<float> distSpeed(m_params.minSpeed, m_params.maxSpeed);
        std::uniform_real_distribution<float> distSpawn(0.0f, m_params.activeDuration);

        float globalTime = (m_state == 2) ? m_timer : (m_params.activeDuration + m_timer);
        int dropCount = (m_mode == RainMode::DualPillar) ? 800 : 400;

        DirectX::XMFLOAT3 pPos = m_target->GetPosition();

        for (int i = 0; i < dropCount; ++i) {
            float speed = distSpeed(gen);
            float spawnTime = distSpawn(gen);
            float localTime = globalTime - spawnTime;

            if (localTime < 0.0f) continue;

            DirectX::XMFLOAT3 dropPos = {};
            bool isActive = false;

            // Kalkulasi matematika murni (Sama persis dengan Render)
            if (m_mode == RainMode::DualPillar) {
                DirectX::XMFLOAT3 activeCenter = (i % 2 == 0) ? m_center : m_center2;
                std::uniform_real_distribution<float> distX(activeCenter.x - halfW, activeCenter.x + halfW);
                float rx = distX(gen);
                float z = (activeCenter.z + halfD + 5.0f) - (localTime * speed);
                if (z >= activeCenter.z - halfD) { dropPos = { rx, 1.0f, z }; isActive = true; }
            }
            else if (m_mode == RainMode::VerticalSweep) {
                std::uniform_real_distribution<float> distX(m_center.x - halfW, m_center.x + halfW);
                float rx = distX(gen);
                float z = (m_center.z + halfD + 5.0f) - (localTime * speed);
                if (z >= m_center.z - halfD) { dropPos = { rx, 1.0f, z }; isActive = true; }
            }
            else { // HorizontalSweep
                std::uniform_real_distribution<float> distZ(m_center.z - halfD, m_center.z + halfD);
                float rz = distZ(gen);
                float startX = (m_sweepDir > 0) ? (m_center.x - halfW - 5.0f) : (m_center.x + halfW + 5.0f);
                float endX = (m_sweepDir > 0) ? (m_center.x + halfW) : (m_center.x - halfW);
                float x = startX + (localTime * speed * m_sweepDir);
                if ((m_sweepDir > 0 && x <= endX) || (m_sweepDir < 0 && x >= endX)) {
                    dropPos = { x, 1.0f, rz }; isActive = true;
                }
            }

            // Jika tetesan hujan ini valid dan ada di layar, cek jaraknya dengan Player
            if (isActive) {
                float dx = pPos.x - dropPos.x;
                float dy = pPos.y - dropPos.y;
                float dz = pPos.z - dropPos.z;

                // Jarak kuadrat 0.49f = Radius Player(0.3) + Hujan(0.4) = 0.7f * 0.7f
                if ((dx * dx + dy * dy + dz * dz) <= 0.49f) {
                    m_target->TakeDamage((int)m_params.damage);
                    CameraController::Instance().AddTrauma(0.15f); // Micro-shake

                    // [PENTING] Break loop agar player tidak terkena ratusan damage dalam 1 frame
                    break;
                }
            }
        }
    }
}

void AttackRain::Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) {
    if (m_state == 0 || !boss || !boss->GetWindowSystem()) return;

    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
    float actualW = GetActualWidth();
    float actualD = GetActualDepth();
    float halfW = actualW * 0.5f;
    float halfD = actualD * 0.5f;

    // --- State 1: Blinking warning rectangle ---
    if (m_state == 1) {
        float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
        int   screenW = GetSystemMetrics(SM_CXSCREEN);
        int   screenH = GetSystemMetrics(SM_CYSCREEN);
        XMFLOAT3 camPos = camera->GetPosition();

        float blink = (sinf(m_timer * 20.0f) + 1.0f) * 0.5f;
        float alpha = 0.2f + (blink * 0.4f);
        float width2D = actualW * p2u;
        float height2D = actualD * p2u;

        auto drawRect = [&](XMFLOAT3 center) {
            float sx = (center.x - camPos.x) * p2u + (screenW * 0.5f);
            float sy = -(center.z - camPos.z) * p2u + (screenH * 0.5f);
            m_zonePrimitive->Rect(sx, sy, width2D, height2D,
                width2D * 0.5f, height2D * 0.5f,
                0.0f, 1.0f, 0.0f, 0.0f, alpha);
            };

        drawRect(m_center);
        if (m_mode == RainMode::DualPillar) drawRect(m_center2);
        m_zonePrimitive->Render(context);
    }

    // --- States 2/3: Active drop rain (deterministic, fixed seed) ---
    if (m_state == 2 || m_state == 3) {
        std::mt19937 gen(1337);
        std::uniform_real_distribution<float> distSpeed(m_params.minSpeed, m_params.maxSpeed);
        std::uniform_real_distribution<float> distSpawn(0.0f, m_params.activeDuration);

        float globalTime = (m_state == 2)
            ? m_timer
            : (m_params.activeDuration + m_timer);

        int dropCount = (m_mode == RainMode::DualPillar) ? 800 : 400;
        if ((int)m_vfxHandles.size() < dropCount)
            m_vfxHandles.resize(dropCount, -1);

        for (int i = 0; i < dropCount; ++i) {
            float speed = distSpeed(gen);
            float spawnTime = distSpawn(gen);
            float localTime = globalTime - spawnTime;
            if (localTime < 0.0f) continue;

            XMFLOAT3 dropPos = {};
            bool      isActive = false;
            float     yawAngle = 0.0f;

            if (m_mode == RainMode::DualPillar) {
                XMFLOAT3 activeCenter = (i % 2 == 0) ? m_center : m_center2;
                std::uniform_real_distribution<float> distX(
                    activeCenter.x - halfW, activeCenter.x + halfW);
                float rx = distX(gen);
                float topEdge = activeCenter.z + halfD + 5.0f;
                float bottomEdge = activeCenter.z - halfD;
                float z = topEdge - (localTime * speed);
                if (z >= bottomEdge) {
                    dropPos = { rx, 1.0f, z };
                    isActive = true;
                    yawAngle = XM_PI;
                }
            }
            else if (m_mode == RainMode::VerticalSweep) {
                std::uniform_real_distribution<float> distX(
                    m_center.x - halfW, m_center.x + halfW);
                float rx = distX(gen);
                float topEdge = m_center.z + halfD + 5.0f;
                float bottomEdge = m_center.z - halfD;
                float z = topEdge - (localTime * speed);
                if (z >= bottomEdge) {
                    dropPos = { rx, 1.0f, z };
                    isActive = true;
                    yawAngle = XM_PI;
                }
            }
            else { // HorizontalSweep
                std::uniform_real_distribution<float> distZ(
                    m_center.z - halfD, m_center.z + halfD);
                float rz = distZ(gen);
                float dir = m_sweepDir;
                float startX = (dir > 0) ? (m_center.x - halfW - 5.0f)
                    : (m_center.x + halfW + 5.0f);
                float endX = (dir > 0) ? (m_center.x + halfW)
                    : (m_center.x - halfW);
                float x = startX + (localTime * speed * dir);
                bool inside = (dir > 0) ? (x <= endX) : (x >= endX);
                if (inside) {
                    dropPos = { x, 1.0f, rz };
                    isActive = true;
                    yawAngle = (dir > 0) ? XM_PIDIV2 : -XM_PIDIV2;
                }
            }

            if (isActive) {
                shapeRenderer->DrawSphere(dropPos, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });

                if (m_vfxHandles[i] == -1) {
                    m_vfxHandles[i] = EffectManager::Instance().Play(
                        "Data/Effect/VFX_Boss_Asgore_Rain.efk", dropPos, 0.4f);
                    float rotX = XMConvertToRadians(90.0f);
                    EffectManager::Instance().SetRotation(
                        m_vfxHandles[i], { rotX, yawAngle, 0.0f });
                }
                else {
                    EffectManager::Instance().SetPosition(m_vfxHandles[i], dropPos);
                }
            }
            else {
                if (m_vfxHandles[i] != -1) {
                    EffectManager::Instance().Stop(m_vfxHandles[i]);
                    m_vfxHandles[i] = -1;
                }
            }
        }
    }
}

void AttackRain::Stop(NaviBoss* boss) {
    ClearVFX();
    m_state = 0;
}

bool AttackRain::IsFinished() const {
    return m_state == 0;
}

float AttackRain::GetActualWidth() const {
    if (m_mode == RainMode::DualPillar)    return 18.0f;
    if (m_mode == RainMode::VerticalSweep) return 25.0f;
    return 80.0f;
}

float AttackRain::GetActualDepth() const {
    if (m_mode == RainMode::VerticalSweep) return 45.0f;
    return 15.0f;
}

void AttackRain::ClearVFX() {
    for (int& h : m_vfxHandles) {
        if (h != -1) { EffectManager::Instance().Stop(h); h = -1; }
    }
}