#pragma once
#include "PlayerState.h"
#include "Player.h"
#include "StateMachine.h"
#include "AnimationController.h" 
#include "System/Input.h" // [WAJIB] Untuk membaca IsTriggered

class PlayerIdle;
class PlayerRun;
class PlayerSprint;
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

class PlayerSprint : public PlayerState {
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
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

// --- IDLE ---
inline void PlayerIdle::Update(Player* player, float dt) {
    if (player->GetMovement()->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerRun());
        return;
    }

    // [LOGIKA TAP vs HOLD]
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        player->dashInputTimer += dt;
    } else {
        // Tombol dilepas cepat (Tap) -> DASH
        if (player->dashInputTimer > 0.0f && player->dashInputTimer <= 0.2f && player->canDash) {
            player->dashInputTimer = 0.0f;
            player->GetStateMachine()->ChangeState(player, new PlayerDash());
            return;
        }
        player->dashInputTimer = 0.0f;
    }
}

// --- RUN (Jalan Biasa) ---
inline void PlayerRun::Enter(Player* player) {
    player->GetAnimator()->Play("RunForwardInPlace", true);
}

// --- RUN (Jalan Biasa) ---
inline void PlayerRun::Update(Player* player, float dt) {
    if (!player->GetMovement()->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        return;
    }

    // [LOGIKA TAP vs HOLD]
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        player->dashInputTimer += dt;

        // Tombol ditahan lama (Hold) -> SPRINT
        if (player->dashInputTimer > 0.2f) {
            player->dashInputTimer = 0.0f; // Reset timer saat masuk sprint
            player->GetStateMachine()->ChangeState(player, new PlayerSprint());
            return;
        }
    }
    else {
        // Tombol dilepas cepat (Tap) -> DASH
        if (player->dashInputTimer > 0.0f && player->dashInputTimer <= 0.2f && player->canDash) {
            player->dashInputTimer = 0.0f;
            player->GetStateMachine()->ChangeState(player, new PlayerDash());
            return;
        }
        player->dashInputTimer = 0.0f;
    }
}

// --- SPRINT (Lari Cepat) ---
inline void PlayerSprint::Enter(Player* player) {
    player->SetMoveSpeed(player->GetSprintSpeed());
    // player->GetAnimator()->Play("SprintAnim", true); // Opsional
}

// --- SPRINT (Lari Cepat) ---
inline void PlayerSprint::Update(Player* player, float dt) {
    // Jika tombol Shift dilepas, kembali lari normal (Run)
    if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        player->dashInputTimer = 0.0f; // Pastikan bersih
        player->GetStateMachine()->ChangeState(player, new PlayerRun());
        return;
    }

    if (!player->GetMovement()->IsMoving()) {
        player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        return;
    }
}

inline void PlayerSprint::Exit(Player* player) {
    // Kembalikan ke speed normal
    player->SetMoveSpeed(player->GetBaseSpeed());
}

// --- DASH (Dorongan Instan) ---
inline void PlayerDash::Enter(Player* player) {
    timer = player->GetDashDuration();
    dashDir = player->GetLastValidInput(); // Dash mengikuti arah terakhir menghadap

    player->canDash = false;
    player->dashCooldownTimer = 0.5f; // Set durasi cooldown

    // player->GetAnimator()->Play("DashAnim", false); // Opsional
}

inline void PlayerDash::Update(Player* player, float dt) {
    timer -= dt;

    // Paksa (Override) Velocity menabrak kalkulasi normal
    player->GetMovement()->SetVelocity(DirectX::XMFLOAT3(
        dashDir.x * player->GetDashSpeed(),
        0.0f,
        dashDir.y * player->GetDashSpeed()
    ));

    // Durasi dash habis
    if (timer <= 0.0f) {
        // Flow Control: Lanjut lari, lari santai, atau diam?
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            player->GetStateMachine()->ChangeState(player, new PlayerSprint());
        }
        else if (player->GetMovement()->IsMoving()) {
            player->GetStateMachine()->ChangeState(player, new PlayerRun());
        }
        else {
            player->GetStateMachine()->ChangeState(player, new PlayerIdle());
        }
    }
}

inline void PlayerDash::Exit(Player* player) {
    // Matikan sisa momentum agar tidak licin setelah dash selesai
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}