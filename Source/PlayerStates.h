#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" // [FIX C2027] Wajib include ini agar bisa panggil ->Play()

// [FIX C2061] Forward Declarations
// Kita beritahu compiler bahwa class ini "akan ada", 
// jadi pointer-nya bisa dikenali.
class PlayerIdle;
class PlayerRun;
//class PlayerJump;

// ==========================================
// 1. CLASS DEFINITIONS (Hanya Deklarasi)
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
// 2. IMPLEMENTATION (Logic di sini)
// ==========================================
// Kita taruh logic di bawah, setelah SEMUA class terdefinisi.
// Dengan begini, PlayerIdle sudah kenal PlayerRun, dan sebaliknya.

// --- IDLE IMPLEMENTATION ---
inline void PlayerIdle::Enter(Player* player)
{
    player->GetAnimator()->Play("Idle", true, 0.1f);
}

inline void PlayerIdle::Update(Player* player, float dt)
{
    player->HandleMovementInput();

    if (player->GetMovement()->IsMoving())
    {
        // Disini compiler sudah tahu apa itu PlayerRun karena definisinya ada di atas
        player->GetStateMachine()->ChangeState(player, new PlayerRun());
    }
    //else if (player->CheckJumpInput())
    //{
    //    player->GetStateMachine()->ChangeState(player, new PlayerJump());
    //}
}

// --- RUN IMPLEMENTATION ---
inline void PlayerRun::Enter(Player* player)
{
    player->GetAnimator()->Play("RunForwardInPlace", true);
}

inline void PlayerRun::Update(Player* player, float dt)
{
    player->HandleMovementInput();

    if (!player->GetMovement()->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
    }
    //else if (player->CheckJumpInput())
    //{
    //    player->GetStateMachine()->ChangeState(player, new PlayerJump());
    //}
}

// --- JUMP IMPLEMENTATION ---
//inline void PlayerJump::Enter(Player* player)
//{
//    player->GetMovement()->Jump();
//    player->GetAnimator()->Play("JumpGoesUp", false, 0.1f);
//}
//
//inline void PlayerJump::Update(Player* player, float dt)
//{
//    player->HandleMovementInput();
//
//    // Logika Animasi Lompat Dinamis
//    float vy = player->GetMovement()->GetVelocity().y;
//
//    if (vy > 2.0f)
//    {
//        player->GetAnimator()->Play("JumpGoesUp", false, 0.2f);
//    }
//    else if (vy >= -2.0f && vy <= 2.0f)
//    {
//        player->GetAnimator()->Play("JumpPeak", true, 0.3f);
//    }
//    else
//    {
//        player->GetAnimator()->Play("JumpGoesDown", false, 0.3f);
//    }
//
//    // Transisi Mendarat
//    if (player->GetMovement()->IsGrounded() && vy <= 0.0f)
//    {
//        if (player->GetMovement()->IsMoving())
//        {
//            player->GetStateMachine()->ChangeState(player, new PlayerRun());
//        }
//        else
//        {
//            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
//        }
//    }
//}