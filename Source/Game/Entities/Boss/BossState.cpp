#include "BossStates.h"
#include "BossAIController.h"
#include "NaviPhaseWindowkill.h"
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"

void State_Idle::Update(float dt, NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) {
    m_cooldown -= dt;
    if (m_cooldown <= 0.0f) {
        // Cooldown finished! Inject a random attack into the phase and switch to Attacking state.
        int attackChoice = rand() % 4;

        if (attackChoice == 0) {
            phase->AddAttack(std::make_unique<AttackBouncing>(phase->GetBouncingParams()));
        }
        else if (attackChoice == 1) {
            phase->AddAttack(std::make_unique<AttackBoomerangs>(phase->GetBoomerangParams()));
        }
        else if (attackChoice == 2) {
            phase->AddAttack(std::make_unique<AttackBlasters>(phase->GetBlasterParams(), false));
        }
        else {
            phase->AddAttack(std::make_unique<AttackSpears>(phase->GetUndyneParams(), phase->GetAITarget()));
        }

        controller->ChangeState(std::make_unique<State_Attacking>(), boss, phase);
    }
}

void State_Attacking::Update(float dt, NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) {
    // If the phase has no active attacks left, the attack is finished. Go back to idle.
    if (!phase->HasActiveAttacks()) {
        controller->ChangeState(std::make_unique<State_Idle>(), boss, phase);
    }
}