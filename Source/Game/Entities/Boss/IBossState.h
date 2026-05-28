#pragma once

class NaviBoss;
class BossAIController;
class INaviPhase; // Forward declaration

class IBossState {
public:
    virtual ~IBossState() = default;

    // Gunakan INaviPhase* agar state ini bisa dipakai oleh fase apa pun
    virtual void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) = 0;
    virtual void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) = 0;
    virtual void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) = 0;
};