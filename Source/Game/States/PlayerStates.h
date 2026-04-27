#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" 
#include "System/Input.h"

// ==========================================
// 1. DEKLARASI CLASS STATE
// ==========================================
class PlayerIdle : public PlayerState {
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerMoving : public PlayerState {
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

class PlayerShoot : public PlayerState {
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override {}
    void Exit(Player* player) override {}
};

class PlayerDamage : public PlayerState {
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override {}
    void Exit(Player* player) override {}
};

class PlayerDead : public PlayerState {
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override {}
    void Exit(Player* player) override {}
};

// ==========================================
// 2. IMPLEMENTASI LOGIKA STATE
// ==========================================

// --- IDLE ---
inline void PlayerIdle::Enter(Player* player) {
    // Pastikan nama "Idle" sudah sesuai dengan yang di-export dari Blender
    player->GetAnimator()->Play("Rig|Idle", true, 0.2f);
}

inline void PlayerIdle::Update(Player* player, float dt) {
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash) {
        player->GetStateMachine()->ChangeState(player, new PlayerDash());
        return;
    }

    // [FIX] Gunakan fungsi IsMoving() buatan kita sendiri
    if (player->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerMoving());
    }
}

// --- MOVING ---
inline void PlayerMoving::Enter(Player* player) {
    // Pastikan nama "RunPistol" sudah sesuai dengan yang di-export dari Blender
    player->GetAnimator()->Play("Rig|RunPistol", true, 0.2f);
}

inline void PlayerMoving::Update(Player* player, float dt) {
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash) {
        player->GetStateMachine()->ChangeState(player, new PlayerDash());
        return;
    }

    // [FIX] Gunakan fungsi IsMoving() buatan kita sendiri
    if (!player->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
    }
}

// --- DASH ---
inline void PlayerDash::Enter(Player* player) {
    timer = player->GetDashDuration();
    dashDir = player->GetLastValidInput();
    player->canDash = false;
    player->dashCooldownTimer = 0.5f;
}

inline void PlayerDash::Update(Player* player, float dt) {
    timer -= dt;
    player->GetMovement()->SetVelocity(DirectX::XMFLOAT3(
        dashDir.x * player->GetDashSpeed(), 0.0f, dashDir.y * player->GetDashSpeed()
    ));

    if (timer <= 0.0f) {
        if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerMoving());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerDash::Exit(Player* player) {
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}