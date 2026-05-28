#include "BossAIController.h"
#include "INaviPhase.h"

void BossAIController::Update(float dt, NaviBoss* boss, INaviPhase* phase) {
    if (m_currentState) {
        m_currentState->Update(dt, boss, this, phase);
    }
}

void BossAIController::ChangeState(std::unique_ptr<IBossState> newState, NaviBoss* boss, INaviPhase* phase) {
    if (m_currentState) {
        m_currentState->Exit(boss, this, phase);
    }
    m_currentState = std::move(newState);
    if (m_currentState) {
        m_currentState->Enter(boss, this, phase);
    }
}