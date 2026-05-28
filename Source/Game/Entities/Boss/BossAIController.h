#pragma once
#include <memory>
#include "IBossState.h"

class NaviBoss;
class BossAIController;
class INaviPhase;

class BossAIController {
public:
    BossAIController() = default;
    ~BossAIController() = default;

    void Update(float dt, NaviBoss* boss, INaviPhase* phase);
    void ChangeState(std::unique_ptr<IBossState> newState, NaviBoss* boss, INaviPhase* phase);

private:
    std::unique_ptr<IBossState> m_currentState;
};