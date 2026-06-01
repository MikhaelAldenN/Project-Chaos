#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "NaviBoss.h"
#include <cmath>

// ========================================================
// IMPLEMENTASI AI PHASE 01
// ========================================================
BossAI_Phase01::BossAI_Phase01(BossPhase01* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase01::Update(float dt, NaviBoss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    // Tunggu sampai bos selesai mengeksekusi serangan saat ini
    if (m_phase->HasActiveAttacks() || m_phase->HasRainActive()) {
        return;
    }

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // Eksekusi serangan berdasarkan indeks urutan
        switch (m_sequenceIndex) {
        case 0:
            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(m_radialParams));
            m_cooldownTimer = 1.5f; // Jeda setelah serangan ini selesai
            break;
        case 1:
        {
            DirectX::XMFLOAT3 pPos = m_target->GetPosition();
            DirectX::XMFLOAT3 bPos = boss->GetPosition();
            float lockedAngle = std::atan2f(pPos.x - bPos.x, pPos.z - bPos.z);
            m_phase->AddPooledAttack(std::make_unique<AttackFan>(m_fanParams, lockedAngle));
            m_cooldownTimer = 1.5f;
            break;
        }
        case 2:
            m_phase->TriggerRain(RainMode::VerticalSweep, m_target->GetPosition().x > 0);
            m_cooldownTimer = 1.0f;
            break;
        case 3:
            m_phase->AddPooledAttack(std::make_unique<AttackPhalanx>(m_phalanxParams, m_target));
            m_cooldownTimer = 2.0f;
            break;
        case 4:
            m_phase->AddPooledAttack(std::make_unique<AttackUltimate>(m_ultimateParams, m_target));
            m_cooldownTimer = 3.0f;
            break;
        }

        // Lanjut ke serangan berikutnya, kalau mentok kembali ke 0 (Loop)
        m_sequenceIndex++;
        if (m_sequenceIndex > 4) m_sequenceIndex = 0;
    }
}

// ========================================================
// IMPLEMENTASI AI PHASE 02
// ========================================================
BossAI_Phase02::BossAI_Phase02(BossPhase02* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase02::Update(float dt, NaviBoss* boss) {
    // Jangan serang player jika masih terjebak di kandang
    if (!m_enabled || !m_target || !m_phase || m_phase->IsPlayerCaged()) return;

    // Tunggu sampai serangan saat ini selesai
    if (m_phase->HasActiveAttacks()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // Eksekusi serangan Windowkill
        switch (m_sequenceIndex) {
        case 0:
            m_phase->AddAttack(std::make_unique<AttackBouncing>(m_bouncingParams));
            m_cooldownTimer = 2.0f;
            break;
        case 1:
            m_phase->AddAttack(std::make_unique<AttackBoomerangs>(m_boomerangParams));
            m_cooldownTimer = 2.0f;
            break;
        case 2:
            // Menembak Laser Orbital ke arah Player
            m_phase->AddAttack(std::make_unique<AttackBlasters>(m_blasterParams, true, m_target->GetPosition().x));
            m_cooldownTimer = 2.0f;
            break;
        case 3:
            m_phase->AddAttack(std::make_unique<AttackSpears>(m_undyneParams, m_target));
            m_cooldownTimer = 2.5f;
            break;
        }

        // Loop
        m_sequenceIndex++;
        if (m_sequenceIndex > 3) m_sequenceIndex = 0;
    }
}