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

// [BARU] Mesin Pengatur Giliran Selang-Seling
BossAI_Phase01::AttackSequence BossAI_Phase01::GetNextTacticianAttack() {
    AttackSequence nextAttack;

    if (m_isNextMainAttack) {
        // Giliran Main Attack
        switch (m_mainAttackIndex) {
        case 0: nextAttack = AttackSequence::Phalanx; break;
        case 1: nextAttack = AttackSequence::Rain; break;
        case 2: nextAttack = AttackSequence::Wave; break;
        case 3: nextAttack = AttackSequence::Meteor; break;
        }
        m_mainAttackIndex = (m_mainAttackIndex + 1) % 4; // Ulangi ke 0 jika sudah mencapai 4
    }
    else {
        // Giliran Filler Attack
        switch (m_fillerAttackIndex) {
        case 0: nextAttack = AttackSequence::Radial; break;
        case 1: nextAttack = AttackSequence::Direct; break;
        case 2: nextAttack = AttackSequence::FanContinuos; break;
        }
        m_fillerAttackIndex = (m_fillerAttackIndex + 1) % 3; // Ulangi ke 0 jika sudah mencapai 3
    }

    // Tukar status agar serangan berikutnya bergantian
    m_isNextMainAttack = !m_isNextMainAttack;

    return nextAttack;
}

void BossAI_Phase01::Update(float dt, Boss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    if (m_phase->HasActiveAttacks() || m_phase->HasRainActive()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        float hpPercent = static_cast<float>(m_phase->GetHP()) / static_cast<float>(m_phase->GetMaxHP());
        bool isEnraged = (hpPercent <= 0.5f);

        // Kunci Pengaman Chaos Mode
        if (isEnraged && (m_currentAttack != AttackSequence::RadialContinuos &&
            m_currentAttack != AttackSequence::FanContinuos &&
            m_currentAttack != AttackSequence::Ultimate &&
            m_currentAttack != AttackSequence::Rain))
        {
            m_currentAttack = AttackSequence::RadialContinuos;
        }

        DirectX::XMFLOAT3 pPos = m_target->GetPosition();
        DirectX::XMFLOAT3 bPos = boss->GetPosition();
        float lockedAngle = std::atan2f(pPos.x - bPos.x, pPos.z - bPos.z);

        switch (m_currentAttack) {

            // ========================================================
            // TACTICIAN MODE (HP > 50%)
            // ========================================================
        case AttackSequence::Direct:
            m_phase->AddPooledAttack(std::make_unique<AttackDirect>(AttackParamManager::Instance().GetDirectParams(), m_target));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Fan:
            m_phase->AddPooledAttack(std::make_unique<AttackFan>(AttackParamManager::Instance().GetFanNormalParams(), lockedAngle, m_target));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Radial:
            m_phase->AddPooledAttack(std::make_unique<AttackRadial>(AttackParamManager::Instance().GetRadialNormalParams()));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Phalanx:
            m_phase->AddPooledAttack(std::make_unique<AttackPhalanx>(AttackParamManager::Instance().GetPhalanxParams(), m_target));
            m_cooldownTimer = 2.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Wave:
            m_phase->AddPooledAttack(std::make_unique<AttackWave>(AttackParamManager::Instance().GetWaveParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Meteor:
            m_phase->AddPooledAttack(std::make_unique<AttackMeteor>(AttackParamManager::Instance().GetMeteorParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

            // ========================================================
            // SHARED & CHAOS MODE (HP <= 50%)
            // ========================================================
        case AttackSequence::Rain:
            if (isEnraged) {
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep, m_target->GetPosition().x > 0);
                m_cooldownTimer = 0.5f;
                m_currentAttack = AttackSequence::Ultimate; // Rute Chaos
            }
            else {
                // RUTE TACTICIAN BARU (RainTargeted)
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainTargetedParams(), RainMode::Targeted, true);

                // Beri cooldown cepat agar Boss langsung menembak serangan "Filler" selagi hujan masih berjatuhan!
                m_cooldownTimer = 1.0f;
                m_currentAttack = GetNextTacticianAttack();
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
            m_currentAttack = AttackSequence::RadialContinuos;
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