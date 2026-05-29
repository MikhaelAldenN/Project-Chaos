#include "BossStates.h"
#include "BossAIController.h"

// Phase headers — hanya include yang dibutuhkan masing-masing state
#include "Boss_Phase01.h"
#include "Boss_Phase02.h"

// Phase 1 attack patterns
#include "Attack_Radial.h"
#include "Attack_Fan.h"
#include "Attack_Phalanx.h"
#include "Attack_Ultimate.h"

// Phase 2 attack patterns
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"


// ============================================================
//  PHASE 1 — State_P1_Idle
// ============================================================

void State_P1_Idle::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Pilih durasi cooldown secara acak saat masuk state
    m_cooldown = m_cooldownDist(m_rng);
}

void State_P1_Idle::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    auto* p1 = dynamic_cast<Boss_Phase01*>(phase);
    if (!p1) return;

    m_cooldown -= dt;
    if (m_cooldown > 0.0f) return;

    // Pilih serangan secara acak
    int choice = m_attackDist(m_rng);

    switch (choice)
    {
    case 0:
        // Triple Radial Burst
        p1->AddPooledAttack(
            std::make_unique<Attack_Radial>(p1->GetRadialParams()));
        break;

    case 1:
        if (p1->GetAITarget())
        {
            DirectX::XMFLOAT3 pPos = p1->GetAITarget()->GetPosition();
            DirectX::XMFLOAT3 bPos = boss->GetPosition();
            float lockedAngle = std::atan2f(pPos.x - bPos.x, pPos.z - bPos.z);

            p1->AddPooledAttack(
                std::make_unique<Attack_Fan>(
                    p1->GetFanParams(),
                    lockedAngle));
        }
        break;

    case 2:
        // Glintstone Phalanx
        if (p1->GetAITarget())
        {
            p1->AddPooledAttack(
                std::make_unique<Attack_Phalanx>(
                    p1->GetPhalanxParams(),
                    p1->GetAITarget()));
        }
        break;

    case 3:
        // Bijuudama (Ultimate)
        if (p1->GetAITarget())
        {
            p1->AddPooledAttack(
                std::make_unique<Attack_Ultimate>(
                    p1->GetUltimateParams(),
                    p1->GetAITarget()));
        }
        break;

    default:
        break;
    }

    controller->ChangeState(std::make_unique<State_P1_Attacking>(), boss, phase);
}

void State_P1_Idle::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tidak ada cleanup khusus
}


// ============================================================
//  PHASE 1 — State_P1_Attacking
// ============================================================

void State_P1_Attacking::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tidak ada setup khusus — attack sudah di-queue oleh P1_Idle
}

void State_P1_Attacking::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tunggu sampai semua attack selesai, lalu kembali ke Idle
    if (!phase->HasActiveAttacks())
    {
        controller->ChangeState(std::make_unique<State_P1_Idle>(), boss, phase);
    }
}

void State_P1_Attacking::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tidak ada cleanup khusus
}


// ============================================================
//  PHASE 2 — State_P2_Idle
// ============================================================

void State_P2_Idle::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    m_cooldown = m_cooldownDist(m_rng);
}

void State_P2_Idle::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    auto* p2 = dynamic_cast<Boss_Phase02*>(phase);
    if (!p2) return;

    // AI tidak berjalan selama player masih di dalam kandang
    if (p2->IsPlayerCaged()) return;

    m_cooldown -= dt;
    if (m_cooldown > 0.0f) return;

    int choice = m_attackDist(m_rng);

    switch (choice)
    {
    case 0:
        p2->AddAttack(
            std::make_unique<AttackBouncing>(p2->GetBouncingParams()));
        break;

    case 1:
        p2->AddAttack(
            std::make_unique<AttackBoomerangs>(p2->GetBoomerangParams()));
        break;

    case 2:
        p2->AddAttack(
            std::make_unique<AttackBlasters>(p2->GetBlasterParams(), false));
        break;

    case 3:
        p2->AddAttack(
            std::make_unique<AttackSpears>(
                p2->GetUndyneParams(),
                p2->GetAITarget()));
        break;

    default:
        break;
    }

    controller->ChangeState(std::make_unique<State_P2_Attacking>(), boss, phase);
}

void State_P2_Idle::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tidak ada cleanup khusus
}


// ============================================================
//  PHASE 2 — State_P2_Attacking
// ============================================================

void State_P2_Attacking::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Attack sudah di-queue oleh P2_Idle
}

void State_P2_Attacking::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    if (!phase->HasActiveAttacks())
    {
        controller->ChangeState(std::make_unique<State_P2_Idle>(), boss, phase);
    }
}

void State_P2_Attacking::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase)
{
    // Tidak ada cleanup khusus
}