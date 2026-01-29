#pragma once
#include <string>

// Forward Declaration (Biar tidak circular dependency)
class Boss;

// ==========================================
// 1. ABSTRACT BASE STATE
// ==========================================
class BossState
{
public:
    virtual ~BossState() = default;

    // Dipanggil sekali saat masuk state (Reset timer, set warna, audio, dll)
    virtual void Enter(Boss* boss) = 0;

    // Logic per frame (Pilih attack, gerak, deteksi player)
    virtual void Update(Boss* boss, float dt) = 0;

    // Dipanggil sekali saat keluar state
    virtual void Exit(Boss* boss) = 0;

    // Opsional: Untuk debug nama state
    virtual std::string GetName() const { return "Unknown"; }
};

// ==========================================
// 2. BOSS STATE MACHINE MANAGER
// ==========================================
class BossStateMachine
{
public:
    ~BossStateMachine();

    void Initialize(BossState* startState, Boss* boss);
    void ChangeState(Boss* boss, BossState* newState);
    void Update(Boss* boss, float dt);

    // Cek apakah state tertentu sedang aktif (berguna untuk logic luar)
    bool IsState(const std::string& stateName) const;

private:
    BossState* currentState = nullptr;
};

// ==========================================
// 3. CONCRETE STATES (DEFINITIONS)
// ==========================================

// State: Intro (Saat Boss baru muncul/spawn)
class BossIntroState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Intro"; }

private:
    float m_timer = 0.0f;
    float m_duration = 3.0f; // Intro selama 3 detik
};

// State: Idle (Menunggu/Mencari Player sebelum menyerang)
class BossIdleState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Idle"; }

private:
    float m_timer = 0.0f;
    float m_decisionTime = 2.0f; // Waktu berpikir sebelum attack
};

// State: Spawn Enemy (Memunculkan Minion)
class BossSpawnEnemyState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Spawn Enemy"; }

private:
    float m_timer = 0.0f;
    float m_duration = 2.0f; // Waktu delay setelah spawn sebelum kembali ke Idle
};