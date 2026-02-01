#include "StateBoss.h"
#include "Boss.h"
#include "EnemyManager.h"
#include <Player.h>
#include <algorithm> // For std::min/max if needed

using namespace DirectX;

// ==========================================
// [AI] STATIC MEMBER DEFINITIONS
// Definisi di sini (bukan di .h) agar linker tidak duplikat.
// ==========================================
int BossIdleState::s_nextAttackIndex = 0;
int BossIdleState::s_cooldowns[static_cast<int>(AttackID::COUNT)] = { 0, 0, 0, 0, 0 };

// ==========================================
// BOSS STATE MACHINE
// ==========================================

BossStateMachine::~BossStateMachine()
{
    if (currentState) { delete currentState; currentState = nullptr; }
}

void BossStateMachine::Initialize(BossState* startState, Boss* boss)
{
    currentState = startState;
    if (currentState) currentState->Enter(boss);
}

void BossStateMachine::ChangeState(Boss* boss, BossState* newState)
{
    if (currentState)
    {
        currentState->Exit(boss);
        delete currentState;
    }
    currentState = newState;
    if (currentState) currentState->Enter(boss);
}

void BossStateMachine::Update(Boss* boss, float dt)
{
    if (currentState) currentState->Update(boss, dt);
}

bool BossStateMachine::IsState(const std::string& stateName) const
{
    return currentState && currentState->GetName() == stateName;
}

// ==========================================
// STATE: INTRO
// ==========================================

void BossIntroState::Enter(Boss* boss)
{
    boss->AddTerminalLog("SYSTEM: INITIALIZING CORE...");
    boss->AddTerminalLog("WARNING: HIGH ENERGY SIGNATURE");
    m_timer = 0.0f;
}

void BossIntroState::Update(Boss* boss, float dt)
{
    m_timer += dt;
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
// STATE: IDLE  (AI DECISION HUB)
// ==========================================
// Cara kerja AI:
//
// 1. ROUND-ROBIN CURSOR (s_nextAttackIndex)
//    Setiap kali Idle dipanggil, AI akan mencoba serangan di index
//    s_nextAttackIndex. Setelah dipilih, index maju +1 (wrap ke 0).
//
// 2. COOLDOWN CHECK
//    Sebelum serangan dipilih, AI cek cooldown-nya. Jika masih > 0,
//    serangan itu DILEWATI dan AI pindah ke index berikutnya.
//    Ini mencegah serangan kuat (misal Pentagon) terlalu sering.
//
// 3. SHUFFLE (30% chance)
//    Setelah serangan valid ditemukan, ada 30% chance AI akan SKIP
//    ke serangan berikutnya yang juga valid. Ini bikin pola tidak
//    terlalu prediktabel tanpa kehilangan priority structure.
//
// 4. COOLDOWN DECREMENT
//    Setiap kali AI berhasil memilih serangan (apapun), SEMUA cooldown
//    yang aktif didekremen sebesar 1. Ini yang bikin cooldown countdown.
//
// 5. COOLDOWN SET
//    Setelah serangan dipilih, cooldown serangan itu di-set ke nilai
//    default-nya (misal Pentagon = 3, artinya butuh 3 serangan lain
//    sebelum Pentagon bisa dipilih lagi).
// ==========================================

void BossIdleState::Enter(Boss* boss)
{
    m_timer = 0.0f;
    boss->GetMonitor1()->ResetToIdle();
    boss->AddTerminalLog("SYSTEM: SCANNING...");
}

void BossIdleState::Update(Boss* boss, float dt)
{
    m_timer += dt;

    if (m_timer < m_decisionTime) return;  // Tunggu delay sebelum memutus

    // =========================================================
    // [AI] STEP 1: CARI SERANGAN VALID DARI ROUND-ROBIN
    // Putar dari s_nextAttackIndex, lewati yang masih di cooldown.
    // =========================================================
    int candidateIndex = -1;
    int searchStart = s_nextAttackIndex;

    for (int i = 0; i < static_cast<int>(AttackID::COUNT); ++i)
    {
        int tryIndex = (searchStart + i) % static_cast<int>(AttackID::COUNT);

        if (s_cooldowns[tryIndex] <= 0)
        {
            candidateIndex = tryIndex;
            break;
        }
    }

    // =========================================================
    // [AI] SAFETY: Kalau semua serangan di cooldown, fallback SpawnEnemy
    // =========================================================
    if (candidateIndex < 0)
    {
        candidateIndex = static_cast<int>(AttackID::SpawnEnemy);
        s_cooldowns[candidateIndex] = 0;
    }

    // =========================================================
    // [AI] STEP 2: CONDITION-BASED WEIGHTING
    // Sebelum shuffle, cek kondisi gameplay dan bias pilihan.
    // Ini yang bikin AI "adaptive"  bukan cuma round-robin.
    // =========================================================
    if (auto* player = boss->GetPlayer())
    {
        XMFLOAT3 pPos = player->GetPosition();

        // Hitung jarak player ke center (0, 0, 0)  proxy untuk "player di mana"
        float distFromCenter = sqrtf(pPos.x * pPos.x + pPos.z * pPos.z);

        // --- BIAS A: Player jauh dari center -> lebih sering Wire Attack ---
        // Wire Attack lebih efektif kalau player ada di pinggir (sulit dodge ke samping)
        if (distFromCenter > 7.0f && s_cooldowns[static_cast<int>(AttackID::WireAttack)] <= 0)
        {
            // 40% chance override ke WireAttack
            if ((rand() % 100) < 40)
            {
                candidateIndex = static_cast<int>(AttackID::WireAttack);
            }
        }

        // --- BIAS B: Player di center -> lebih sering Lock Player ---
        // Lock Player paling devastating kalau player di tengah (banyak musuh di sekitar)
        if (distFromCenter < 4.0f && s_cooldowns[static_cast<int>(AttackID::LockPlayer)] <= 0)
        {
            // 35% chance override ke LockPlayer
            if ((rand() % 100) < 35)
            {
                candidateIndex = static_cast<int>(AttackID::LockPlayer);
            }
        }
    }

    // =========================================================
    // [AI] STEP 3: SHUFFLE CHANCE (55%)
    // Skip ke serangan valid berikutnya.
    // =========================================================
    if ((rand() % 100) < static_cast<int>(k_shuffleChance * 100.0f))
    {
        for (int i = 1; i < static_cast<int>(AttackID::COUNT); ++i)
        {
            int tryIndex = (candidateIndex + i) % static_cast<int>(AttackID::COUNT);
            if (s_cooldowns[tryIndex] <= 0)
            {
                candidateIndex = tryIndex;
                break;
            }
        }
    }

    // =========================================================
    // [AI] STEP 4: UPDATE ROUND-ROBIN CURSOR
    // =========================================================
    s_nextAttackIndex = (candidateIndex + 1) % static_cast<int>(AttackID::COUNT);

    // =========================================================
    // [AI] STEP 5: DEKREMENT SEMUA COOLDOWN YANG AKTIF
    // =========================================================
    for (int i = 0; i < static_cast<int>(AttackID::COUNT); ++i)
    {
        if (s_cooldowns[i] > 0) s_cooldowns[i]--;
    }

    // =========================================================
    // [AI] STEP 6: SET COOLDOWN SERANGAN YANG DIPILIH
    // =========================================================
    AttackID chosenAttack = static_cast<AttackID>(candidateIndex);

    switch (chosenAttack)
    {
    case AttackID::SpawnEnemy:      s_cooldowns[candidateIndex] = k_cooldownSpawnEnemy;      break;
    case AttackID::SpawnPentagon:   s_cooldowns[candidateIndex] = k_cooldownSpawnPentagon;   break;
    case AttackID::LockPlayer:      s_cooldowns[candidateIndex] = k_cooldownLockPlayer;      break;
    case AttackID::DownloadAttack:  s_cooldowns[candidateIndex] = k_cooldownDownloadAttack;  break;
    case AttackID::WireAttack:      s_cooldowns[candidateIndex] = k_cooldownWireAttack;      break;
    }

    // =========================================================
    // [AI] STEP 7: EKSEKUSI SERANGAN
    // =========================================================
    switch (chosenAttack)
    {
    case AttackID::SpawnEnemy:
        boss->AddTerminalLog("AI: SELECTED -> SPAWN ENEMY");
        boss->TriggerSpawnEnemy();
        break;

    case AttackID::SpawnPentagon:
        boss->AddTerminalLog("AI: SELECTED -> SPAWN PENTAGON");
        boss->TriggerSpawnPentagon();
        break;

    case AttackID::LockPlayer:
        boss->AddTerminalLog("AI: SELECTED -> LOCK PLAYER");
        boss->TriggerLockPlayer();
        break;

    case AttackID::DownloadAttack:
        boss->AddTerminalLog("AI: SELECTED -> DOWNLOAD ATTACK");
        boss->TriggerDownloadAttack();
        break;

    case AttackID::WireAttack:
        boss->AddTerminalLog("AI: SELECTED -> WIRE ATTACK");
        boss->TriggerWireAttackState();
        break;
    }
}

void BossIdleState::Exit(Boss* boss) {}

// ==========================================
// STATE: SPAWN ENEMY (SEQUENCE)
// ==========================================

void BossSpawnEnemyState::Enter(Boss* boss)
{
    boss->AddTerminalLog("EXEC: THREAD_FORK [6]");

    // Set timer LEBIH BESAR dari max interval (1.5f)
    // Supaya pas masuk state, spawn pertama langsung keluar (Instant)
    m_spawnTimer = 2.0f;

    m_currentSpawnCount = 0;

    // Animasi Typing Awal
    boss->GetMonitor1()->PlayCommandAnimation("INSTANCE: PADDLE_UNIT(0)");
}

void BossSpawnEnemyState::Update(Boss* boss, float dt)
{
    // Jika masih ada jatah spawn
    if (m_currentSpawnCount < m_totalSpawns)
    {
        m_spawnTimer += dt;

        // 1. Dynamic Interval (Makin cepat)
        float t = (float)m_currentSpawnCount / (float)(m_totalSpawns - 1);
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        float currentInterval = m_startInterval + (m_endInterval - m_startInterval) * t;

        if (m_spawnTimer >= currentInterval)
        {
            m_spawnTimer = 0.0f;

            if (auto* em = boss->GetEnemyManager())
            {
                // --- A. TENTUKAN POSISI AWAL (POLA) ---
                // Pola spread untuk 6 musuh: kiri/kanan alternating,
                // dengan variasi Z yang lebih luas agar tidak numpuk.
                float xOffset = (m_currentSpawnCount % 2 == 0) ? -10.0f : 10.0f;

                float zPos = 0.0f;
                switch (m_currentSpawnCount)
                {
                case 0: zPos = -7.0f;  break;  // Bawah kiri
                case 1: zPos = -3.0f;  break;  // Bawah kanan
                case 2: zPos = 9.0f;  break;  // Atas kiri
                case 3: zPos = 5.0f;  break;  // Tengah kanan
                case 4: zPos = -9.0f;  break;  // Bawah jauh kiri
                case 5: zPos = 7.0f;  break;  // Atas jauh kanan
                default: zPos = 0.0f;  break;
                }

                XMFLOAT3 finalSpawnPos = { xOffset, 0.0f, zPos };

                // --- B. SAFETY CHECK (ANTI-SPAWN KILL) ---
                // Cek jarak ke Player. Jika terlalu dekat, pindah sisi!
                if (auto* player = boss->GetPlayer())
                {
                    XMFLOAT3 pPos = player->GetPosition();

                    // Hitung Jarak Kuadrat (biar hemat akar)
                    float dx = finalSpawnPos.x - pPos.x;
                    float dz = finalSpawnPos.z - pPos.z;
                    float distSq = dx * dx + dz * dz;

                    // Batas Aman: 8.0 unit (8x8 = 64)
                    if (distSq < 64.0f)
                    {
                        // TERLALU DEKAT! -> Lempar ke seberang X
                        finalSpawnPos.x = -finalSpawnPos.x;

                        boss->AddTerminalLog("WARN: SPAWN_REROUTE [PROXIMITY]");
                    }
                }

                // --- C. SPAWN MUSUH ---
                EnemySpawnConfig config;
                config.Position = finalSpawnPos;
                config.Color = { 1.f, 0.f, 0.f, 1.f };
                config.Type = EnemyType::Paddle;
                config.AttackBehavior = AttackType::Tracking;
                config.Direction = MoveDir::None;

                em->SpawnEnemy(config);

                // --- D. UPDATE INFO ---
                m_currentSpawnCount++;
                std::string cmd = "INSTANCE: PADDLE_UNIT(" + std::to_string(m_currentSpawnCount) + ")";
                boss->GetMonitor1()->SetTextImmediate(cmd);
                boss->AddTerminalLog("FORK: PROCESS_ID_" + std::to_string(m_currentSpawnCount));
            }
        }
    }
    else
    {
        m_spawnTimer += dt;
        if (m_spawnTimer >= m_exitDelay)
        {
            boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
        }
    }
}

void BossSpawnEnemyState::Exit(Boss* boss)
{
    boss->AddTerminalLog("THREAD_FORK: COMPLETE");
}

// ==========================================
// STATE: LOCK PLAYER
// ==========================================

void BossLockPlayerState::Enter(Boss* boss)
{
    boss->AddTerminalLog("WARNING: GRAVITY WELL DETECTED!");
    boss->AddTerminalLog("MOBILITY SYSTEMS: OFFLINE");
    boss->GetMonitor1()->ShowLockScreen();
    m_timer = 0.0f;
}

void BossLockPlayerState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    Player* player = boss->GetPlayer();

    if (player)
    {
        // Phase 1: Magnetic Pull (4.0s - 4.4s)
        if (m_timer >= 4.0f && m_timer < m_introDuration)
        {
            player->SetMovementLock(true);

            // Interpolate player to center
            XMFLOAT3 cur = player->GetPosition();
            float lerp = 10.0f * dt;

            player->SetPosition(
                cur.x + (m_lockPosition.x - cur.x) * lerp,
                0.0f,
                cur.z + (m_lockPosition.z - cur.z) * lerp
            );
        }
        // Phase 2: Hard Lock (4.4s+)
        else if (m_timer >= m_introDuration)
        {
            player->SetMovementLock(true);
            player->SetPosition(m_lockPosition.x, 0.0f, m_lockPosition.z);
        }
    }

    if (m_timer >= (m_introDuration + m_holdDuration))
    {
        boss->AddTerminalLog("GRAVITY WELL: DISSIPATED");
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossLockPlayerState::Exit(Boss* boss)
{
    if (auto* p = boss->GetPlayer())
    {
        p->SetMovementLock(false);
        boss->AddTerminalLog("MOBILITY SYSTEMS: RESTORED");
    }
    boss->GetMonitor1()->ResetToIdle();
}

// ==========================================
// STATE: COMMAND WRAPPER
// ==========================================

BossCommandState::~BossCommandState()
{
    if (m_nextState) { delete m_nextState; m_nextState = nullptr; }
}

void BossCommandState::Enter(Boss* boss)
{
    boss->GetMonitor1()->PlayCommandAnimation(m_message);
    boss->AddTerminalLog("EXEC: " + m_message);
    m_waitTimer = 0.0f;
}

void BossCommandState::Update(Boss* boss, float dt)
{
    if (boss->GetMonitor1()->IsBusy()) return;

    m_waitTimer += dt;
    if (m_waitTimer > 0.5f)
    {
        // Transfer ownership of nextState to the machine
        BossState* next = m_nextState;
        m_nextState = nullptr;
        boss->GetStateMachine()->ChangeState(boss, next);
    }
}

void BossCommandState::Exit(Boss* boss) {}

// ==========================================
// STATE: DOWNLOAD ATTACK
// ==========================================

void BossDownloadAttackState::Enter(Boss* boss)
{
    boss->GetMonitor1()->PlayCommandAnimation("DOWNLOADING FILES...");
    boss->AddTerminalLog("PROTOCOL: DATA_SIPHON");

    // Randomize Layout
    m_isMirrored = (rand() % 2 != 0);
    boss->AddTerminalLog(m_isMirrored ? "CONFIG: MIRRORED_LAYOUT" : "CONFIG: STANDARD_LAYOUT");

    m_timer = 0.0f;
    m_spawnTimer = 0.0f;

    // Reset Antenna using Config from Boss
    if (BossPart* antenna = boss->GetPart("antenna"))
    {
        // Calculate start position based on Mirror + Config
        float startX = m_isMirrored ? -boss->m_antennaConfig.hiddenPos.x : boss->m_antennaConfig.hiddenPos.x;
        antenna->position = { startX, boss->m_antennaConfig.hiddenPos.y, boss->m_antennaConfig.hiddenPos.z };
        antenna->useFloating = false;
    }
}

void BossDownloadAttackState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    BossPart* antenna = boss->GetPart("antenna");

    // Configs from Boss
    const auto& cfg = boss->m_antennaConfig;

    // Calculate Targets based on Mirror Status
    XMFLOAT3 targetHidden = cfg.hiddenPos;
    XMFLOAT3 targetActive = cfg.showPos;
    XMFLOAT3 targetRot = cfg.rotation;

    if (m_isMirrored)
    {
        targetHidden.x = -targetHidden.x;
        targetActive.x = -targetActive.x;

        // Hardcoded Flip Rotation for "Mirrored" Look
        targetRot.x = 445.0f;
        targetRot.y = 472.0f;
        targetRot.z = 101.0f;
    }

    // --- PHASE 1: SLIDE IN ---
    if (m_timer < m_durationSlideIn)
    {
        if (antenna)
        {
            float t = m_timer / m_durationSlideIn;
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Position
            XMVECTOR vHidden = XMLoadFloat3(&targetHidden);
            XMVECTOR vActive = XMLoadFloat3(&targetActive);
            XMVECTOR vPos = XMVectorLerp(vHidden, vActive, smoothT);
            XMStoreFloat3(&antenna->position, vPos);

            antenna->rotation = targetRot;
        }
    }
    // --- PHASE 2: ATTACK ---
    else if (m_timer < (m_durationSlideIn + m_durationAttack))
    {
        if (antenna) {
            antenna->position = targetActive;
            antenna->rotation = targetRot;
        }

        m_spawnTimer += dt;
        if (m_spawnTimer > 0.08f)
        {
            m_spawnTimer = 0.0f;

            // Target Tip Calculation
            XMFLOAT3 target = targetActive;
            target.y += 4.0f;
            target.x += m_isMirrored ? 0.5f : -0.5f;

            // Spawn Source Calculation (Read from Config)
            XMFLOAT3 spawnPos = cfg.spawnSrc;
            if (m_isMirrored) spawnPos.x = -spawnPos.x;

            // Add Randomness
            float spreadZ = 8.0f;
            spawnPos.z += (((rand() % 200) / 100.0f) - 1.0f) * spreadZ;
            spawnPos.x += (((rand() % 100) / 50.0f) - 1.0f);

            boss->SpawnFileProjectile(spawnPos, target);
        }
    }
    // --- PHASE 3: SLIDE OUT ---
    else if (m_timer < (m_durationSlideIn + m_durationAttack + m_durationSlideOut))
    {
        if (antenna)
        {
            float exitTime = m_timer - (m_durationSlideIn + m_durationAttack);
            float t = exitTime / m_durationSlideOut;
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Backwards
            XMVECTOR vHidden = XMLoadFloat3(&targetHidden);
            XMVECTOR vActive = XMLoadFloat3(&targetActive);
            XMVECTOR vPos = XMVectorLerp(vActive, vHidden, smoothT);
            XMStoreFloat3(&antenna->position, vPos);
        }
    }
    else
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }

    // --- PROJECTILE LOGIC ---
    auto& projectiles = boss->GetProjectiles();
    float safeDt = min(dt, 0.05f);

    for (auto& p : projectiles)
    {
        if (!p.active) continue;

        XMVECTOR vPos = XMLoadFloat3(&p.position);
        XMVECTOR vTarget = XMLoadFloat3(&p.targetPos);
        XMVECTOR vDiff = XMVectorSubtract(vTarget, vPos);
        float dist = XMVectorGetX(XMVector3Length(vDiff));
        float step = p.speed * safeDt;

        if (dist < step)
        {
            p.active = false;
        }
        else
        {
            XMVECTOR vDir = XMVectorScale(XMVector3Normalize(vDiff), step);
            XMStoreFloat3(&p.position, XMVectorAdd(vPos, vDir));

            p.rotation.x += 360.0f * dt;
            p.rotation.y += 180.0f * dt;
        }
    }
}

void BossDownloadAttackState::Exit(Boss* boss)
{
    boss->AddTerminalLog("DOWNLOAD: COMPLETE (100%)");
    boss->ClearProjectiles();

    // Reset Antenna Position
    if (BossPart* antenna = boss->GetPart("antenna"))
    {
        antenna->position = boss->m_antennaConfig.hiddenPos;
    }
    boss->GetMonitor1()->ResetToIdle();
}

// ==========================================
// STATE: SPAWN PENTAGON
// ==========================================

void BossSpawnPentagonState::Enter(Boss* boss)
{
    boss->AddTerminalLog("WARNING: HEAVY UNIT DETECTED");
    boss->AddTerminalLog("PROTOCOL: RADIAL_BURST");

    if (auto* em = boss->GetEnemyManager())
    {
        EnemySpawnConfig config;

        // Read config from Boss (Updated via ImGui)
        config.Position = boss->m_pentagonConfig.pos;
        float s = boss->m_pentagonConfig.scale;
        config.Scale = { s, s, s };

        config.Rotation = { 0.f, 0.f, 0.f };
        config.Color = { 1.f, 0.f, 1.f, 1.f };
        config.Type = EnemyType::Pentagon;
        config.AttackBehavior = AttackType::RadialBurst;
        config.Direction = MoveDir::None;

        em->SpawnEnemy(config);
        boss->AddTerminalLog("UNIT DEPLOYED: PENTAGON (STATIC)");
    }
    m_timer = 0.0f;
}

void BossSpawnPentagonState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossSpawnPentagonState::Exit(Boss* boss) {}

// ==========================================
// STATE: WIRE ATTACK
// ==========================================

void BossWireAttackState::Enter(Boss* boss)
{
    // Pesan log saat fase dimulai (setelah ngetik selesai)
    boss->AddTerminalLog("WARNING: VOLTAGE SURGE");
    boss->AddTerminalLog("SYSTEM: DISCHARGING...");

    // Reset timer
    m_timer = 0.0f;
    m_attackIntervalTimer = 0.0f;

    // Tembakan pertama langsung saat mulai
    boss->TriggerWireAttack();
}

void BossWireAttackState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    m_attackIntervalTimer += dt;

    // Logika Spawn Kabel berkala
    if (m_attackIntervalTimer >= m_attackInterval)
    {
        m_attackIntervalTimer = 0.0f; // Reset interval timer
        boss->TriggerWireAttack();    // Panggil fungsi mekanik di Boss.cpp
        boss->AddTerminalLog("SYSTEM: ARC DISCHARGE");
    }

    // Cek apakah durasi state habis
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossWireAttackState::Exit(Boss* boss)
{
    boss->AddTerminalLog("SYSTEM: VOLTAGE STABILIZED");
    // Opsional: Kalau mau boss kembali ke pose idle spesifik
    boss->GetMonitor1()->ResetToIdle();
}