#pragma execution_character_set("utf-8")
#include "AttackFan.h"
#include "Boss.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

AttackFan::AttackFan(const FanParams& params, float lockedBaseAngle)
    : m_params(params), m_lockedBaseAngle(lockedBaseAngle) {}

void AttackFan::StartPooled(Boss* boss, std::vector<std::unique_ptr<Bullet>>* pool) {
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_wavesFired = 0;

    // Set timer to full delay so first wave fires immediately on first Update()
    m_waveTimer = m_params.waveDelay;
}

void AttackFan::Update(float dt, Boss* boss) {
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

void AttackFan::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    // Bullets are rendered by the phase's central render loop
}

void AttackFan::Stop(Boss* boss) {
    m_active = false;
}

bool AttackFan::IsFinished() const {
    return !m_active;
}

// ============================================================
// Internal
// ============================================================

void AttackFan::FireWave(Boss* boss) {
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