#include "BossAIController.h"
#include "NaviPhaseWindowkill.h" // [FIX] Phaseの情報を取得するために必要

void BossAIController::Update(float dt, NaviBoss* boss, NaviPhaseWindowkill* phase) { // [FIX] クラス名と { を追加
    if (m_currentState) {
        m_currentState->Update(dt, boss, this, phase);
    }
}

void BossAIController::ChangeState(std::unique_ptr<IBossState> newState, NaviBoss* boss, NaviPhaseWindowkill* phase) {
    if (m_currentState) {
        m_currentState->Exit(boss, this, phase);
    }
    m_currentState = std::move(newState);
    if (m_currentState) {
        m_currentState->Enter(boss, this, phase);
    }
}