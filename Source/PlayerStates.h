#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" 

// Forward Declarations
class PlayerIdle;
class PlayerRun;
//class PlayerJump;

// ==========================================
// 1. CLASS DEFINITIONS
// ==========================================

class PlayerIdle : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerRun : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerJump : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

// ==========================================
// 2. IMPLEMENTATION
// ==========================================

// --- IDLE IMPLEMENTATION ---
inline void PlayerIdle::Enter(Player* player)
{
    player->GetAnimator()->Play("Idle", true, 0.1f);
}

inline void PlayerIdle::Update(Player* player, float dt)
{
    // [FIX] Masukkan 'dt' ke dalam fungsi ini!
    player->HandleMovementInput(dt);

    if (player->GetMovement()->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, new PlayerRun());
    }
}

// --- RUN IMPLEMENTATION ---
inline void PlayerRun::Enter(Player* player)
{
    player->GetAnimator()->Play("RunForwardInPlace", true);
}

inline void PlayerRun::Update(Player* player, float dt)
{
    // [FIX] Masukkan 'dt' ke dalam fungsi ini!
    player->HandleMovementInput(dt);

    if (!player->GetMovement()->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
    }
}

// --- JUMP IMPLEMENTATION ---
// (Jika nanti kamu mengaktifkan lompat, jangan lupa tambahkan 'dt' di sini juga)
/*
inline void PlayerJump::Update(Player* player, float dt)
{
    player->HandleMovementInput(dt); // <--- Jangan lupa 'dt'

    // ... logic lompat ...
}
*/