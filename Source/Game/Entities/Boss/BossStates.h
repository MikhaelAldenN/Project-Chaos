// BossStates.h — SESUDAH
#pragma once
#include "IBossState.h"
#include <random>

class NaviBoss;        // forward declaration eksplisit
class BossAIController; // forward declaration eksplisit

// State: Boss floating around, waiting for cooldown to finish.
class State_Idle : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
private:
    float m_cooldown = 0.0f;
};

class State_Attacking : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
};