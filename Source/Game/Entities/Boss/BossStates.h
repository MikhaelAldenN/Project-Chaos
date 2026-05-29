#pragma once
#include "IBossState.h"
#include <random>

class NaviBoss;
class BossAIController;

// ============================================================
// BossStates.h
//
// Semua AI state untuk Phase 1 dan Phase 2 dikumpulkan di
// sini agar tetap dalam satu file, tapi penamaannya dipisah
// dengan prefix:
//   State_P1_  → dipakai oleh Boss_Phase01
//   State_P2_  → dipakai oleh Boss_Phase02
//
// Masing-masing state hanya boleh di-cast ke phase yang sesuai.
// ============================================================


// ============================================================
//  PHASE 1 STATES
//  Digunakan oleh Boss_Phase01.
//  State machine: P1_Idle → P1_Attacking → P1_Idle → ...
//  Rain berjalan di track terpisah dan tidak masuk state ini.
// ============================================================

// State: Boss melayang, menunggu cooldown selesai sebelum
// memilih serangan berikutnya.
class State_P1_Idle : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;

private:
    float m_cooldown = 0.0f;

    // RNG disimpan sebagai member agar tidak di-seed ulang tiap frame
    std::mt19937                     m_rng{ std::random_device{}() };
    std::uniform_int_distribution<int> m_attackDist{ 0, 3 };
    std::uniform_real_distribution<float> m_cooldownDist{ 1.5f, 3.0f };
};

// State: Boss sedang mengeksekusi serangan.
// Menunggu sampai semua active attacks selesai, lalu kembali ke Idle.
class State_P1_Attacking : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
};


// ============================================================
//  PHASE 2 STATES
//  Digunakan oleh Boss_Phase02.
//  State machine: P2_Idle → P2_Attacking → P2_Idle → ...
//  AI tidak aktif selama player masih terkurung di kandang.
// ============================================================

// State: Boss menunggu cooldown sebelum melempar serangan
// Windowkill berikutnya. Memilih attack secara acak.
class State_P2_Idle : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;

private:
    float m_cooldown = 0.0f;

    std::mt19937                       m_rng{ std::random_device{}() };
    std::uniform_int_distribution<int> m_attackDist{ 0, 3 };
    std::uniform_real_distribution<float> m_cooldownDist{ 1.8f, 3.5f };
};

// State: Boss sedang mengeksekusi serangan Windowkill.
// Menunggu semua attack selesai sebelum kembali ke P2_Idle.
class State_P2_Attacking : public IBossState {
public:
    void Enter(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Update(float dt, NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
    void Exit(NaviBoss* boss, BossAIController* controller, INaviPhase* phase) override;
};