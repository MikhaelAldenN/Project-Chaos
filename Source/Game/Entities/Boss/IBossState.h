#pragma once

class NaviBoss;
class BossAIController;
class NaviPhaseWindowkill; // [FIX] ‚±‚Ì‘O•ûéŒ¾‚ª•K{‚Å‚·

// Represents a single state in the boss's behavior (e.g., Idle, Attacking, Stunned).
class IBossState {
public:
    virtual ~IBossState() = default;

    // Called once when the state becomes active.
    virtual void Enter(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) = 0;

    // Called every frame. Returns true if the state has finished its logic.
    virtual void Update(float dt, NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) = 0;

    // Called once before transitioning to another state.
    virtual void Exit(NaviBoss* boss, BossAIController* controller, NaviPhaseWindowkill* phase) = 0;
};