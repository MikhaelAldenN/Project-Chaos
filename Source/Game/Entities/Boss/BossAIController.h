#pragma once
#include <memory>
#include "IBossState.h"

class NaviBoss;
class NaviPhaseWindowkill; // [FIX] ‘O•ûéŒ¾‚ğ’Ç‰Á

// Manages the current state of the boss and handles transitions.
class BossAIController {
public:
    BossAIController() = default;
    ~BossAIController() = default;

    // Updates the currently active state.
    void Update(float dt, NaviBoss* boss, NaviPhaseWindowkill* phase);

    // Transitions to a new state safely.
    void ChangeState(std::unique_ptr<IBossState> newState, NaviBoss* boss, NaviPhaseWindowkill* phase);

private:
    std::unique_ptr<IBossState> m_currentState;
};