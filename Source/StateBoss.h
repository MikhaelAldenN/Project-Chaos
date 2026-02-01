#pragma once
#include <string>
#include <DirectXMath.h>

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

// State: Player Lock
class BossLockPlayerState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Lock Player"; }

private:
    float m_timer = 0.0f;
    float m_duration = 5.0f; // Berapa lama player dikunci

    // Titik tengah arena tempat player ditarik
    DirectX::XMFLOAT3 m_lockPosition = { 0.0f, 0.0f, -8.0f };
    float m_pullSpeed = 5.0f;
};

class BossCommandState : public BossState
{
public:
    // Constructor menerima: State Tujuan (nextState) dan Pesan (message)
    BossCommandState(BossState* nextState, const std::string& message)
        : m_nextState(nextState), m_message(message) {}

    // Destructor: PENTING! Jika state ini dihancurkan sebelum pindah,
    // kita harus hapus m_nextState agar tidak memory leak.
    ~BossCommandState()
    {
        if (m_nextState) {
            delete m_nextState;
            m_nextState = nullptr;
        }
    }

    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Command: " + m_message; }

private:
    BossState* m_nextState = nullptr;
    std::string m_message;
    float m_waitTimer = 0.0f;
    bool m_animStarted = false;
};

// [BARU] State: Download Attack
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

    // Konfigurasi Timeline
    float m_durationSlideIn = 1.0f;
    float m_durationAttack = 5.0f;
    float m_durationSlideOut = 1.0f;

    // Konfigurasi Posisi Antena (Slide)
    DirectX::XMFLOAT3 m_antennaHiddenPos = { 30.5f, 0.0f, -8.7f }; // Pojok Kanan Bawah (Luar Layar)
    DirectX::XMFLOAT3 m_antennaActivePos = { 21.5f, 0.0f, -8.7f };  // Pojok Kanan Bawah (Dalam Layar)

    // Konfigurasi Posisi Spawn File
    DirectX::XMFLOAT3 m_fileSpawnSource = { -19.0f, 6.0f, 11.2f }; // Pojok Kiri Atas (Luar Layar)

    bool m_isMirrored = false;
};

class BossSpawnPentagonState : public BossState
{
public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float dt) override;
    void Exit(Boss* boss) override;
    std::string GetName() const override { return "Spawn Pentagon"; }

private:
    float m_timer = 0.0f;
    float m_duration = 2.0f; // Jeda sebelum kembali ke Idle
};