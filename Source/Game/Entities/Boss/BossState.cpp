#include "BossStates.h"
#include "BossAIController.h"
#include "NaviPhaseWindowkill.h"
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"

void State_Idle::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    auto* wkPhase = dynamic_cast<NaviPhaseWindowkill*>(phase);
    if (!wkPhase) return; // Keamanan ganda

    m_cooldown -= dt;
    if (m_cooldown <= 0.0f) {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(0, 3);
        int attackChoice = dist(rng);

        if (attackChoice == 0) {
            wkPhase->AddAttack(std::make_unique<AttackBouncing>(wkPhase->GetBouncingParams()));
        }
        else if (attackChoice == 1) {
            wkPhase->AddAttack(std::make_unique<AttackBoomerangs>(wkPhase->GetBoomerangParams()));
        }
        else if (attackChoice == 2) {
            wkPhase->AddAttack(std::make_unique<AttackBlasters>(wkPhase->GetBlasterParams(), false));
        }
        else {
            wkPhase->AddAttack(std::make_unique<AttackSpears>(wkPhase->GetUndyneParams(), wkPhase->GetAITarget()));
        }

        controller->ChangeState(std::make_unique<State_Attacking>(), boss, phase);
    }
}

void State_Attacking::Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    if (!phase->HasActiveAttacks()) {
        controller->ChangeState(std::make_unique<State_Idle>(), boss, phase);
    }
}

// BossState.cpp — TAMBAHKAN implementasi kosong yang wajib ada

void State_Idle::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    m_cooldown = 2.0f; // reset cooldown saat masuk state
}

void State_Idle::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    // tidak ada cleanup khusus
}

void State_Attacking::Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    // tidak ada setup khusus
}

void State_Attacking::Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) {
    // tidak ada cleanup khusus
}