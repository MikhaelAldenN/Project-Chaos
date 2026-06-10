#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "Boss.h"
#include <cmath>

// ========================================================
// IMPLEMENTASI AI PHASE 01
// ========================================================
BossAI_Phase01::BossAI_Phase01(BossPhase01* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase01::Update(float dt, Boss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    // Tunggu sampai bos selesai mengeksekusi serangan saat ini
    if (m_phase->HasActiveAttacks() || m_phase->HasRainActive()) {
        return;
    }

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // 1. Cek rasio HP Boss
        float hpPercent = static_cast<float>(m_phase->GetHP()) / static_cast<float>(m_phase->GetMaxHP());
        bool isEnraged = (hpPercent <= 0.5f);

        // Kunci Pengaman Transisi: 
        // Jika HP turun di bawah 50%, paksa lompat keluar dari rotasi santai (Radial/Fan/Phalanx)
        if (isEnraged && (m_currentAttack == AttackSequence::Radial ||
            m_currentAttack == AttackSequence::Fan ||
            m_currentAttack == AttackSequence::Phalanx))
        {
            m_currentAttack = AttackSequence::RadialContinuos;
        }

        // Kalkulasi posisi pemain untuk serangan yang butuh aim
        DirectX::XMFLOAT3 pPos = m_target->GetPosition();
        DirectX::XMFLOAT3 bPos = boss->GetPosition();
        float lockedAngle = std::atan2f(pPos.x - bPos.x, pPos.z - bPos.z);

        // Eksekusi serangan berdasarkan Enum
        switch (m_currentAttack) {

            // ========================================================
            // TACTICIAN MODE (HP > 50%)
            // ========================================================
        case AttackSequence::Radial:
            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(m_radialParams));
            m_cooldownTimer = 1.5f;
            m_currentAttack = AttackSequence::Fan; // Antrean berikutnya
            break;

        case AttackSequence::Fan:
            m_phase->AddPooledAttack(std::make_unique<AttackFan>(m_fanParams, lockedAngle));
            m_cooldownTimer = 1.5f;
            m_currentAttack = AttackSequence::Phalanx;
            break;

        case AttackSequence::Phalanx:
            m_phase->AddPooledAttack(std::make_unique<AttackPhalanx>(m_phalanxParams, m_target));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Radial; // Loop normal kembali ke awal
            break;


            // ========================================================
            // CHAOS MODE (HP <= 50%)
            // ========================================================
        case AttackSequence::RadialContinuos:
        {
            // COMBO: Stream Air Mancur 3 Detik
            RadialParams streamParams = m_radialParams;
            streamParams.activeDuration = 3.0f;
            streamParams.burstDelay = 0.3f;

            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(streamParams));
            m_cooldownTimer = 3.5f; // Jeda sebanding dengan durasi stream
            m_currentAttack = AttackSequence::FanContinuos;
            break;
        }

        case AttackSequence::FanContinuos:
        {
            FanParams burstParams = m_fanParams;
            burstParams.triggerCount = 3;
            burstParams.waves = 4;
            burstParams.waveDelay = 0.12f;

            m_phase->AddPooledAttack(std::make_unique<AttackFan>(burstParams, lockedAngle, m_target));

            m_cooldownTimer = 1.0f;
            m_currentAttack = AttackSequence::Rain;
            break;
        }

        case AttackSequence::Rain:
            // Area denial
            m_phase->TriggerRain(RainMode::VerticalSweep, m_target->GetPosition().x > 0);
            m_cooldownTimer = 0.5f; // Rain tidak mengunci pergerakan bos, langsung eksekusi Ultimate
            m_currentAttack = AttackSequence::Ultimate;
            break;

        case AttackSequence::Ultimate:
            m_phase->AddPooledAttack(std::make_unique<AttackUltimate>(m_ultimateParams, m_target));
            m_cooldownTimer = 2.5f;
            m_currentAttack = AttackSequence::RadialContinuos; // Loop chaos kembali ke awal
            break;
        }
    }
}

// ========================================================
// IMPLEMENTASI AI PHASE 02
// ========================================================
BossAI_Phase02::BossAI_Phase02(BossPhase02* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase02::Update(float dt, Boss* boss) {
    // Jangan serang player jika masih terjebak di kandang
    if (!m_enabled || !m_target || !m_phase || m_phase->IsPlayerCaged()) return;

    // Tunggu sampai serangan saat ini selesai
    if (m_phase->HasActiveAttacks()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // Eksekusi serangan Windowkill
        switch (m_currentAttack) {
        case AttackSequence::Bouncing:
            m_phase->AddAttack(std::make_unique<AttackBouncing>(m_bouncingParams));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Boomerang;
            break;

        case AttackSequence::Boomerang:
            m_phase->AddAttack(std::make_unique<AttackBoomerangs>(m_boomerangParams));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Blaster;
            break;

        case AttackSequence::Blaster:
            // Menembak Laser Orbital ke arah Player
            m_phase->AddAttack(std::make_unique<AttackBlasters>(m_blasterParams, true, m_target->GetPosition().x));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Spear;
            break;

        case AttackSequence::Spear:
            m_phase->AddAttack(std::make_unique<AttackSpears>(m_undyneParams, m_target));
            m_cooldownTimer = 2.5f;
            m_currentAttack = AttackSequence::Bouncing;
            break;
        }
    }
}