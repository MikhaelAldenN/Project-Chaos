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

// --- Idle State (AI Decision Hub) ---
class BossIdleState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Idle"; }

private:
    // =========================================================
    // [AI] ATTACK INDEX ENUM
    // Urutan sesuai priority yang diberikan user (tinggi ke rendah).
    // Nilai enum = index ke dalam array cooldown & nama serangan.
    // =========================================================
    enum class AttackID
    {
        SpawnEnemy = 0,    // Priority 1 (paling sering)
        SpawnPentagon = 1,    // Priority 2
        LockPlayer = 2,    // Priority 3
        DownloadAttack = 3,    // Priority 4
        WireAttack = 4,    // Priority 5 (paling jarang)
        COUNT = 5     // Total jumlah serangan (sentinel)
    };

    // =========================================================
    // [AI] ROUND-ROBIN STATE
    // static biar nilainya PERSISTEN di antara setiap masuk/keluar
    // BossIdleState. Jika ini instance baru (setelah serangan selesai
    // dan kembali ke Idle), index tetap lanjut dari terakhir.
    // =========================================================
    static int s_nextAttackIndex;   // Index serangan berikutnya (round-robin cursor)

    // =========================================================
    // [AI] COOLDOWN SYSTEM
    // Setiap serangan punya cooldown dalam satuan "jumlah idle cycle".
    // Misal cooldown[SpawnPentagon] = 2 berarti setelah Pentagon dipilih,
    // dia tidak akan dipilih lagi sampai 2 serangan lain sudah jalan dulu.
    // Nilai = sisa cooldown yang masih tersisa (didekremen tiap kali AI
    // memilih serangan APAPUN).
    // =========================================================
    static int s_cooldowns[static_cast<int>(AttackID::COUNT)];

    // =========================================================
    // [AI] COOLDOWN DEFAULTS
    // Berapa lama (dalam jumlah serangan lain) sebelum serangan ini
    // boleh dipilih lagi setelah digunakan.
    // =========================================================
    static constexpr int k_cooldownSpawnEnemy = 0;  // Bisa langsung ulang
    static constexpr int k_cooldownSpawnPentagon = 3;  // Tunggu 3 serangan lain
    static constexpr int k_cooldownLockPlayer = 2;  // Tunggu 2 serangan lain
    static constexpr int k_cooldownDownloadAttack = 2;  // Tunggu 2 serangan lain
    static constexpr int k_cooldownWireAttack = 1;  // Tunggu 1 serangan lain

    // =========================================================
    // [AI] SHUFFLE CHANCE
    // Setelah serangan dipilih dari round-robin, ada probabilitas ini
    // untuk SKIP ke serangan berikutnya (menghindari pola terlalu
    // prediktabel). Nilai 0.0 = pure round-robin, 1.0 = pure random.
    // =========================================================
    static constexpr float k_shuffleChance = 0.3f;  // 30% chance skip

    // =========================================================
    // TIMER (non-static, reset setiap masuk state)
    // =========================================================
    float m_timer = 0.0f;
    const float m_decisionTime = 2.0f;  // Delay sebelum AI memilih serangan
};

// --- Spawn Minions State ---
class BossSpawnEnemyState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Spawn Enemy Sequence"; }

private:
    float m_spawnTimer = 0.0f;
    int m_currentSpawnCount = 0;

    // Config Baru
    const int m_totalSpawns = 4;        // Total musuh
    const float m_startInterval = 1.5f; // Delay awal (lambat)
    const float m_endInterval = 0.5f;   // Delay akhir (cepat)

    const float m_exitDelay = 1.0f;     // Jeda setelah selesai
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

class BossWireAttackState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Wire Attack"; }

private:
    float m_timer = 0.0f;
    float m_attackIntervalTimer = 0.0f;

    // Konfigurasi Serangan
    const float m_duration = 8.0f;       // Total durasi fase serangan
    const float m_attackInterval = 1.5f; // Jeda antar tembakan kabel
};