#pragma execution_character_set("utf-8")
#include "AttackFan.h"
#include "NaviBoss.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

Attack_Fan::Attack_Fan(const FanParams& params, float lockedBaseAngle)
    : m_params(params), m_lockedBaseAngle(lockedBaseAngle) {}

void Attack_Fan::StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) {
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_wavesFired = 0;

    // Set timer to full delay so first wave fires immediately on first Update()
    m_waveTimer = m_params.waveDelay;
}

void Attack_Fan::Update(float dt, NaviBoss* boss) {
    if (!m_active || !m_pool) return;

    m_waveTimer += dt;
    if (m_waveTimer >= m_params.waveDelay) {
        m_waveTimer -= m_params.waveDelay;
        FireWave(boss);

        if (++m_wavesFired >= m_params.waves) {
            m_active = false;
        }
    }
}

void Attack_Fan::Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) {
    // Bullets are rendered by the phase's central render loop
}

void Attack_Fan::Stop(NaviBoss* boss) {
    m_active = false;
}

bool Attack_Fan::IsFinished() const {
    return !m_active;
}

// ============================================================
// Internal
// ============================================================

void Attack_Fan::FireWave(NaviBoss* boss) {
    if (!m_pool || !boss) return;

    int   fired = 0;
    float startAngle = m_lockedBaseAngle - ((m_params.lines - 1) * m_params.spreadAngle * 0.5f);

    for (auto& bullet : *m_pool) {
        if (!bullet->IsActive()) {
            bullet->SetRadius(0.25f);
            bullet->scale = { 1.0f, 1.0f, 1.0f };
            bullet->SetHomingTarget(nullptr);
            bullet->SetBossTarget(nullptr);
            bullet->SetParabolic(false);
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);
            bullet->SetDamage(m_params.damage);

            float angle = startAngle + (fired * m_params.spreadAngle);
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.speed);

            if (++fired >= m_params.lines) break;
        }
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}