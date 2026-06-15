#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "Boss.h"
#include "AttackParamManager.h" // <--- JANGAN LUPA INCLUDE INI
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

        // Eksekusi serangan berdasarkan Enum (MENGGUNAKAN PARAM MANAGER)
        switch (m_currentAttack) {

            // ========================================================
            // TACTICIAN MODE (HP > 50%)
            // RUTE BARU: Phalanx -> Rain -> Radial -> Fan
            // ========================================================
        case AttackSequence::Phalanx:
            m_phase->AddPooledAttack(std::make_unique<AttackPhalanx>(AttackParamManager::Instance().GetPhalanxParams(), m_target));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Rain; // 1. Lanjut ke Rain
            break;

        case AttackSequence::Radial:
            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(AttackParamManager::Instance().GetRadialNormalParams()));
            m_cooldownTimer = 1.5f;
            m_currentAttack = AttackSequence::Fan; // 3. Lanjut ke Fan
            break;

        case AttackSequence::Fan:
            m_phase->AddPooledAttack(std::make_unique<AttackFan>(AttackParamManager::Instance().GetFanNormalParams(), lockedAngle));
            m_cooldownTimer = 1.5f;
            m_currentAttack = AttackSequence::Phalanx; // 4. Loop kembali ke Phalanx
            break;

            // ========================================================
            // SHARED & CHAOS MODE (HP <= 50%)
            // ========================================================
        case AttackSequence::Rain:
            // Area denial (Sekarang dipakai di Normal dan Chaos)
            m_phase->TriggerRain(RainMode::VerticalSweep, m_target->GetPosition().x > 0);
            m_cooldownTimer = 0.5f;

            // 2. CEK CABANG: Tentukan serangan berikutnya berdasarkan HP!
            if (isEnraged) {
                m_currentAttack = AttackSequence::Ultimate; // Rute Chaos
            }
            else {
                m_currentAttack = AttackSequence::Radial;   // Rute Normal
            }
            break;

        case AttackSequence::RadialContinuos:
            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(AttackParamManager::Instance().GetRadialContinuousParams()));
            m_cooldownTimer = 3.5f;
            m_currentAttack = AttackSequence::FanContinuos;
            break;

        case AttackSequence::FanContinuos:
            m_phase->AddPooledAttack(std::make_unique<AttackFan>(AttackParamManager::Instance().GetFanContinuousParams(), lockedAngle, m_target));
            m_cooldownTimer = 1.0f;
            m_currentAttack = AttackSequence::Rain;
            break;

        case AttackSequence::Ultimate:
            m_phase->AddPooledAttack(std::make_unique<AttackUltimate>(AttackParamManager::Instance().GetUltimateParams(), m_target));
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
        // Eksekusi serangan Windowkill (MENGGUNAKAN PARAM MANAGER)
        switch (m_currentAttack) {
        case AttackSequence::Bouncing:
            m_phase->AddAttack(std::make_unique<AttackBouncing>(AttackParamManager::Instance().GetBouncingParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Boomerang;
            break;

        case AttackSequence::Boomerang:
            m_phase->AddAttack(std::make_unique<AttackBoomerangs>(AttackParamManager::Instance().GetBoomerangParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Blaster;
            break;

        case AttackSequence::Blaster:
            // Menembak Laser Orbital ke arah Player
            m_phase->AddAttack(std::make_unique<AttackBlasters>(AttackParamManager::Instance().GetBlasterParams(), true, m_target->GetPosition().x));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Spear;
            break;

        case AttackSequence::Spear:
            m_phase->AddAttack(std::make_unique<AttackSpears>(AttackParamManager::Instance().GetUndyneParams(), m_target));
            m_cooldownTimer = 2.5f;
            m_currentAttack = AttackSequence::Bouncing;
            break;
        }
    }
}