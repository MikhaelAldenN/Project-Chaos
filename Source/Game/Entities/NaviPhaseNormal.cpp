#include "NaviPhaseNormal.h"
#include "NaviBoss.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include <SDL3/SDL.h>

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    // (Logika Borderless Fullscreen sama)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowResizable(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, 0, 0);
        SDL_SetWindowSize(sdlWin, screenW, screenH);
    }

    m_bulletPool.clear();
    m_bulletPool.reserve(200);
    for (int i = 0; i < 200; ++i) {
        auto b = std::make_unique<Bullet>();
        b->SetActive(false);
        m_bulletPool.push_back(std::move(b));
    }
}

void NaviPhaseNormal::TriggerSingleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true; m_isDoubleBurst = false;
        m_burstsFired = 0; m_burstSequenceTimer = 0.0f;
    }
}

void NaviPhaseNormal::TriggerDoubleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true; m_isDoubleBurst = true;
        m_burstsFired = 0; m_burstSequenceTimer = 0.0f;
    }
}

// =========================================================
// [NEW] LOGIKA PENGUNCIAN TARGET (LOCK-ON)
// =========================================================
void NaviPhaseNormal::TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos) {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiringFan = true;
        m_fanWavesFired = 0;
        m_fanTimer = m_params.fanWaveDelay; // Dibuat penuh agar langsung menembak di frame 1

        DirectX::XMFLOAT3 bossPos = boss->GetPosition();
        float dx = playerPos.x - bossPos.x;
        float dz = playerPos.z - bossPos.z;

        // Kunci sudut arah player pada saat tombol ditekan
        m_lockedBaseAngle = atan2f(dx, dz);
    }
}

void NaviPhaseNormal::Update(float dt, NaviBoss* boss) {
    if (!boss) return;

    // --- 1A. Logika Radial Burst ---
    if (m_isFiring) {
        if (m_burstsFired == 0) {
            FireRadialBurst(boss, 0.0f);
            m_burstsFired++;
            if (!m_isDoubleBurst) m_isFiring = false;
        }
        else if (m_isDoubleBurst && m_burstsFired == 1) {
            m_burstSequenceTimer += dt;
            if (m_burstSequenceTimer >= m_params.burstDelay) {
                float step = DirectX::XM_2PI / (float)m_params.count;
                FireRadialBurst(boss, step * 0.5f);
                m_isFiring = false;
            }
        }
    }

    // --- 1B. Logika Targeted Fan Burst ---
    if (m_isFiringFan) {
        m_fanTimer += dt;
        if (m_fanTimer >= m_params.fanWaveDelay) {
            m_fanTimer = 0.0f;
            FireFanWave(boss); // Muntahkan 1 baris peluru
            m_fanWavesFired++;

            if (m_fanWavesFired >= m_params.fanWaves) {
                m_isFiringFan = false; // Berhenti jika sudah 5x
            }
        }
    }

    // --- 2. Update & Recycle ---
    float distSq = m_params.despawnDist * m_params.despawnDist;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) continue;
        bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 bossPos = boss->GetPosition();
        float dx = bossPos.x - bPos.x;
        float dz = bossPos.z - bPos.z;
        if ((dx * dx + dz * dz) > distSq) bullet->SetActive(false);
    }
}

// =========================================================
// [NEW] MATEMATIKA FAN / SHOTGUN SPREAD
// =========================================================
void NaviPhaseNormal::FireFanWave(NaviBoss* boss) {
    int lines = m_params.fanLines;
    float spread = m_params.fanSpreadAngle;

    // Cari sudut paling ujung kiri (Start Angle) agar seimbang di tengah
    float startAngle = m_lockedBaseAngle - ((lines - 1) * spread / 2.0f);

    int firedCount = 0;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            float currentAngle = startAngle + (firedCount * spread);
            DirectX::XMFLOAT3 dir = { sinf(currentAngle), 0.0f, cosf(currentAngle) };

            bullet->Fire(boss->GetPosition(), dir, m_params.speed);

            firedCount++;
            if (firedCount >= lines) break; // Berhenti jika sudah menembak 3-4 peluru
        }
    }
}

void NaviPhaseNormal::FireRadialBurst(NaviBoss* boss, float angleOffset) {
    int firedCount = 0;
    float angleStep = DirectX::XM_2PI / (float)m_params.count;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            float angle = (firedCount * angleStep) + angleOffset;
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.speed);
            if (++firedCount >= m_params.count) break;
        }
    }
}

void NaviPhaseNormal::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera) return;
    auto renderer = Graphics::Instance().GetModelRenderer();
    for (auto& bullet : m_bulletPool) {
        if (bullet->IsActive()) renderer->Draw(ShaderId::Phong, bullet->GetModel(), m_params.color);
    }
}

void NaviPhaseNormal::Exit(NaviBoss* boss) { m_bulletPool.clear(); }