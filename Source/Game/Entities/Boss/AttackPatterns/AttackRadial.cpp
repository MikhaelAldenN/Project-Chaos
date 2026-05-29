#pragma execution_character_set("utf-8")
#include "AttackRadial.h"
#include "NaviBoss.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>

using namespace DirectX;

Attack_Radial::Attack_Radial(const RadialParams& params)
    : m_params(params) {}

void Attack_Radial::StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) {
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_burstsFired = 0;
    m_burstTimer = 0.0f;

    // Fire first burst immediately on start
    FireBurst(boss, 0.0f);
    m_burstsFired++;
}

void Attack_Radial::Update(float dt, NaviBoss* boss) {
    if (!m_active || !m_pool) return;

    if (m_burstsFired >= m_params.burstCount) {
        m_active = false;
        return;
    }

    m_burstTimer += dt;
    if (m_burstTimer >= m_params.burstDelay) {
        m_burstTimer -= m_params.burstDelay;

        // Alternate half-step offsets to create an interlocking pattern
        float step = XM_2PI / (float)m_params.count;
        float offset = (m_burstsFired % 2 == 1) ? (step * 0.5f) : 0.0f;

        FireBurst(boss, offset);
        m_burstsFired++;
    }
}

void Attack_Radial::Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) {
    // Bullets are rendered by the phase's central render loop
}

void Attack_Radial::Stop(NaviBoss* boss) {
    m_active = false;
}

bool Attack_Radial::IsFinished() const {
    return !m_active;
}

// ============================================================
// Internal
// ============================================================

void Attack_Radial::FireBurst(NaviBoss* boss, float angleOffset) {
    if (!m_pool || !boss) return;

    int   fired = 0;
    float angleStep = XM_2PI / (float)m_params.count;

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

            float angle = (fired * angleStep) + angleOffset;
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.speed);

            if (++fired >= m_params.count) break;
        }
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}