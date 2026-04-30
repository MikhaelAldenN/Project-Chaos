#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" 
#include "System/Input.h"

class PlayerIdle;
class PlayerMoving;
class PlayerDash;
class PlayerSlash;
class PlayerParry;
class PlayerShoot;

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

class PlayerSlash : public PlayerState {
private:
    float timer = 0.15f;
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
};

class PlayerParry : public PlayerState {
private:
    float timer = 0.2f;
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
};

class PlayerShoot : public PlayerState {
private:
    float timer = 0.15f;
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
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
    player->GetAnimator()->Play("Idle", true, 0.2f);
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
    player->GetAnimator()->Play("RunPistol", true, 0.2f);
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

// --- SLASH ---
inline void PlayerSlash::Enter(Player* player) {
    player->color = { 1.0f, 0.0f, 0.0f, 1.0f }; // Flash Red
    float yawRad = DirectX::XMConvertToRadians(player->GetMovement()->GetRotation().y);
    float dirX = sinf(yawRad);
    float dirZ = cosf(yawRad);
    float lungeForce = 40.0f;

    player->GetMovement()->SetVelocity({
        dirX * lungeForce,
        0.0f,
        dirZ * lungeForce
        });
}

inline void PlayerSlash::Update(Player* player, float dt) {
    timer -= dt;

    DirectX::XMFLOAT3 currentVel = player->GetMovement()->GetVelocity();
    currentVel.x *= 0.7f;
    currentVel.z *= 0.7f;
    player->GetMovement()->SetVelocity(currentVel);

    if (timer <= 0.0f) {
        if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerMoving());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerSlash::Exit(Player* player) {
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Reset color
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}

// --- PARRY ---
inline void PlayerParry::Enter(Player* player) {
    player->color = { 0.0f, 0.5f, 1.0f, 1.0f }; // Flash Blue
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f }); // Stop moving
    player->GetAnimator()->PlayUpper("Parry", false);
}

inline void PlayerParry::Update(Player* player, float dt) {
    timer -= dt;
    if (timer <= 0.0f) {
        if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerMoving());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerParry::Exit(Player* player) {
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Reset color
}

// --- SHOOT ---
inline void PlayerShoot::Enter(Player* player) {
    player->color = { 1.0f, 1.0f, 0.0f, 1.0f }; // Flash Yellow
}

inline void PlayerShoot::Update(Player* player, float dt) {
    timer -= dt;
    if (timer <= 0.0f) {
        if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerMoving());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerShoot::Exit(Player* player) {
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Reset color
}