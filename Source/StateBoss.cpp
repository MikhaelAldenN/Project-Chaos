#include "StateBoss.h"
#include "Boss.h"
#include "EnemyManager.h"
#include <Player.h>
#include <algorithm> // For std::min/max if needed

using namespace DirectX;

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
// STATE: IDLE
// ==========================================

void BossIdleState::Enter(Boss* boss)
{
    m_timer = 0.0f;
    boss->GetMonitor1()->ResetToIdle();
}

void BossIdleState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    if (m_timer >= m_decisionTime)
    {
        boss->AddTerminalLog("ANALYZING COMBAT DATA...");
        m_timer = 0.0f; // Loop for now
    }
}

void BossIdleState::Exit(Boss* boss) {}

// ==========================================
// STATE: SPAWN ENEMY (MINIONS)
// ==========================================

void BossSpawnEnemyState::Enter(Boss* boss)
{
    boss->AddTerminalLog("PROTOCOL: REINFORCEMENT...");

    if (auto* em = boss->GetEnemyManager())
    {
        EnemySpawnConfig config;
        config.Position = { 0.f, 0.f, 0.f };
        config.Color = { 1.f, 0.f, 0.f, 1.f };
        config.Type = EnemyType::Paddle;
        config.AttackBehavior = AttackType::Tracking;
        config.Direction = MoveDir::None;
        config.MinX = -1.0f; config.MaxX = 1.0f;
        config.MinZ = -1.0f; config.MaxZ = 1.0f;

        em->SpawnEnemy(config);
        boss->AddTerminalLog("UNIT DEPLOYED: [0,0,0]");
    }
    else
    {
        boss->AddTerminalLog("ERROR: ENEMY MANAGER DISCONNECTED");
    }
    m_timer = 0.0f;
}

void BossSpawnEnemyState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossSpawnEnemyState::Exit(Boss* boss)
{
    boss->AddTerminalLog("PROTOCOL: COMPLETE.");
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