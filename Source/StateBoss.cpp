#include "StateBoss.h"
#include "Boss.h" // Kita butuh akses penuh ke Boss di sini
#include "EnemyManager.h"

// ==========================================
// BOSS STATE MACHINE IMPLEMENTATION
// ==========================================

BossStateMachine::~BossStateMachine()
{
    if (currentState)
    {
        delete currentState;
        currentState = nullptr;
    }
}

void BossStateMachine::Initialize(BossState* startState, Boss* boss)
{
    currentState = startState;
    if (currentState)
    {
        currentState->Enter(boss);
    }
}

void BossStateMachine::ChangeState(Boss* boss, BossState* newState)
{
    if (currentState)
    {
        currentState->Exit(boss);
        delete currentState;
    }

    currentState = newState;
    if (currentState)
    {
        currentState->Enter(boss);
    }
}

void BossStateMachine::Update(Boss* boss, float dt)
{
    if (currentState)
    {
        currentState->Update(boss, dt);
    }
}

bool BossStateMachine::IsState(const std::string& stateName) const
{
    if (currentState)
    {
        return currentState->GetName() == stateName;
    }
    return false;
}

// ==========================================
// STATE: INTRO
// ==========================================

void BossIntroState::Enter(Boss* boss)
{
    boss->AddTerminalLog("SYSTEM: INITIALIZING CORE...");
    boss->AddTerminalLog("WARNING: HIGH ENERGY SIGNATURE");

    // Reset timer
    m_timer = 0.0f;
}

void BossIntroState::Update(Boss* boss, float dt)
{
    m_timer += dt;

    // Contoh Visual: Buat boss naik perlahan atau glowing
    // (Nanti kita isi logic animasi part di sini)

    // Setelah durasi selesai, pindah ke IDLE
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossIntroState::Exit(Boss* boss)
{
    boss->AddTerminalLog("SYSTEM: ONLINE. TARGET ACQUIRED.");
}

// ==========================================
// STATE: IDLE
// ==========================================

void BossIdleState::Enter(Boss* boss)
{
    // Reset timer berpikir
    m_timer = 0.0f;

    // Contoh: Set warna normal (jika ada fungsi set color)
    // boss->SetColor({1,1,1,1}); 
}

void BossIdleState::Update(Boss* boss, float dt)
{
    m_timer += dt;

    // Boss "bernafas" atau floating idle
    // Logic floating sudah ada di BossPart::Update, jadi biarkan saja.

    // Setelah berpikir X detik, lakukan serangan
    if (m_timer >= m_decisionTime)
    {
        // Nanti di sini kita random mau attack apa
        // boss->GetStateMachine()->ChangeState(boss, new BossAttackLaser());

        // Untuk sekarang, loop log saja sebagai tanda dia mikir
        boss->AddTerminalLog("ANALYZING COMBAT DATA...");
        m_timer = 0.0f; // Reset lagi (Loop idle)
    }
}

void BossIdleState::Exit(Boss* boss)
{
    // Persiapan sebelum masuk state attack
}


// ==========================================
// STATE: SPAWN ENEMY
// ==========================================
// State: Spawn Enemy (Memunculkan Minion)
void BossSpawnEnemyState::Enter(Boss* boss)
{
    boss->AddTerminalLog("PROTOCOL: REINFORCEMENT...");

    // 1. Cek apakah EnemyManager ada
    EnemyManager* em = boss->GetEnemyManager();
    if (em)
    {
        // 2. Konfigurasi Musuh (Paddle yang menembak)
        EnemySpawnConfig config;
        config.Position = { 0.0f, 0.0f, 0.0f }; // Spawn di titik 0
        config.Rotation = { 0.0f, 0.0f, 0.0f }; // Menghadap default
        config.Color = { 1.0f, 0.0f, 0.0f, 1.0f }; // Merah (Biar kelihatan jahat)
        config.Type = EnemyType::Paddle;
        config.AttackBehavior = AttackType::Tracking; // [PENTING] Agar menembak player

        // Parameter patrol (diam di tempat tapi nengok-nengok)
        config.Direction = MoveDir::None;
        config.MinX = -1.0f; config.MaxX = 1.0f; // Area gerak sempit
        config.MinZ = -1.0f; config.MaxZ = 1.0f;

        // 3. Spawn!
        em->SpawnEnemy(config);

        boss->AddTerminalLog("UNIT DEPLOYED: [0,0,0]");
    }
    else
    {
        boss->AddTerminalLog("ERROR: ENEMY MANAGER NOT LINKED");
    }

    m_timer = 0.0f;
}

void BossSpawnEnemyState::Update(Boss* boss, float dt)
{
    m_timer += dt;

    // Tunggu sebentar (animasi spawn selesai), lalu balik ke Idle
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossSpawnEnemyState::Exit(Boss* boss)
{
    boss->AddTerminalLog("PROTOCOL: COMPLETE.");
}