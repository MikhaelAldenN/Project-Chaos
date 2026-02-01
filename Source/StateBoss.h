#pragma once
#include <string>
#include <DirectXMath.h>

// Forward Declaration
class Boss;

// ==========================================
// 1. ABSTRACT BASE STATE
// ==========================================
class BossState
{
public:
    virtual ~BossState() = default;

    // Called once when entering the state
    virtual void Enter(Boss* boss) = 0;

    // Per-frame logic
    virtual void Update(Boss* boss, float dt) = 0;

    // Called once when exiting the state
    virtual void Exit(Boss* boss) = 0;

    // Debug helper
    virtual std::string GetName() const { return "Unknown"; }
};

// ==========================================
// 2. BOSS STATE MACHINE
// ==========================================
class BossStateMachine
{
public:
    ~BossStateMachine();

    void Initialize(BossState* startState, Boss* boss);
    void ChangeState(Boss* boss, BossState* newState);
    void Update(Boss* boss, float dt);
    bool IsState(const std::string& stateName) const;

private:
    BossState* currentState = nullptr;
};

// ==========================================
// 3. CONCRETE STATES
// ==========================================

// --- Intro State ---
class BossIntroState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Intro"; }
private:
    float m_timer = 0.0f;
    const float m_duration = 3.0f;
};

// --- Idle State ---
class BossIdleState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Idle"; }
private:
    float m_timer = 0.0f;
    const float m_decisionTime = 2.0f;
};

// --- Spawn Minions State ---
class BossSpawnEnemyState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Spawn Enemy"; }
private:
    float m_timer = 0.0f;
    const float m_duration = 2.0f;
};

// --- Lock Player State ---
class BossLockPlayerState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Lock Player"; }
private:
    float m_timer = 0.0f;
    const float m_introDuration = 4.4f; // Time until "SNAP"
    const float m_holdDuration = 5.0f;

    const DirectX::XMFLOAT3 m_lockPosition = { 0.0f, 0.0f, -8.0f };
};

// --- Command/Typing State (Transition Wrapper) ---
class BossCommandState : public BossState
{
public:
    BossCommandState(BossState* nextState, const std::string& message)
        : m_nextState(nextState), m_message(message) {}

    ~BossCommandState(); // Handles cleanup of pending nextState

    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Command: " + m_message; }

private:
    BossState* m_nextState = nullptr; // Owns this pointer until transition
    std::string m_message;
    float m_waitTimer = 0.0f;
};

// --- Download Attack State (Antenna) ---
class BossDownloadAttackState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Download Attack"; }

private:
    float m_timer = 0.0f;
    float m_spawnTimer = 0.0f;
    bool m_isMirrored = false;

    // Timeline configuration
    const float m_durationSlideIn = 1.0f;
    const float m_durationAttack = 5.0f;
    const float m_durationSlideOut = 1.0f;
};

// --- Spawn Pentagon State ---
class BossSpawnPentagonState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Spawn Pentagon"; }
private:
    float m_timer = 0.0f;
    const float m_duration = 2.0f;
};