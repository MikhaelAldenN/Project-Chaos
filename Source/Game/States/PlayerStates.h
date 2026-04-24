#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" 
#include "System/Input.h"

class PlayerIdle;
class PlayerRun;
class PlayerDash;

// ==========================================
// 1. CLASS DEFINITIONS
// ==========================================

class PlayerIdle : public PlayerState {
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerRun : public PlayerState {
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerDash : public PlayerState {
private:
    float timer = 0.0f;
    DirectX::XMFLOAT2 dashDir = { 0, 0 };
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
};


// ==========================================
// 2. IMPLEMENTATION
// ==========================================

// --- IDLE ---
inline void PlayerIdle::Enter(Player* player) {
    player->GetAnimator()->Play("Idle", true, 0.1f);
}

inline void PlayerIdle::Update(Player* player, float dt) {
    // Dash Instan (Sangat responsif)
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash) {
        player->GetStateMachine()->ChangeState(player, new PlayerDash());
        return;
    }

    if (player->GetMovement()->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerRun());
    }
}

// --- RUN (Jalan Biasa) ---
inline void PlayerRun::Enter(Player* player) {
    player->GetAnimator()->Play("RunForwardInPlace", true);
}

inline void PlayerRun::Update(Player* player, float dt) {
    // Dash Instan dari posisi lari
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash) {
        player->GetStateMachine()->ChangeState(player, new PlayerDash());
        return;
    }

    if (!player->GetMovement()->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
    }
}

// --- DASH (Dorongan Instan) ---
inline void PlayerDash::Enter(Player* player) {
    timer = player->GetDashDuration();
    dashDir = player->GetLastValidInput();

    player->canDash = false;
    player->dashCooldownTimer = 0.5f;
}

inline void PlayerDash::Update(Player* player, float dt) {
    timer -= dt;

    // Paksa Velocity untuk Dash
    player->GetMovement()->SetVelocity(DirectX::XMFLOAT3(
        dashDir.x * player->GetDashSpeed(),
        0.0f,
        dashDir.y * player->GetDashSpeed()
    ));

    // Durasi dash habis, kembali ke State sebelumnya
    if (timer <= 0.0f) {
        if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerRun());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerDash::Exit(Player* player) {
    // Matikan momentum agar presisi (tidak meluncur)
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}