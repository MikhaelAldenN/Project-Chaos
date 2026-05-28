#pragma once
#include "IBossState.h"
#include <random>

class NaviPhaseWindowkill; // [FIX] èzä¬éQè∆ÇñhÇÆÇΩÇﬂÇÃêÈåæ

// State: Boss floating around, waiting for cooldown to finish.
class State_Idle : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override {
        m_cooldown = 1.5f + ((rand() % 150) / 100.0f); // Random CD between 1.5s - 3.0s
    }

    void Update(float dt, NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override {}

private:
    float m_cooldown = 0.0f;
};

// State: Boss executing an attack pattern.
class State_Attacking : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override {}
    void Update(float dt, NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) override {}
};