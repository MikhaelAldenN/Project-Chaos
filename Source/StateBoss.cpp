#include "StateBoss.h"
#include "Boss.h" // Kita butuh akses penuh ke Boss di sini
#include "EnemyManager.h"
#include <Player.h>

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
    boss->GetMonitor1()->ResetToIdle();
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

// ==========================================
// STATE: LOCK PLAYER
// ==========================================

void BossLockPlayerState::Enter(Boss* boss)
{
    boss->AddTerminalLog("WARNING: GRAVITY WELL DETECTED!");
    boss->AddTerminalLog("MOBILITY SYSTEMS: OFFLINE");

    // Trigger animasi monitor (Total intro sekitar 4.4 detik)
    boss->GetMonitor1()->ShowLockScreen();

    // Reset timer ke 0 (MULAI DARI NOL)
    m_timer = 0.0f;

    // Set total durasi state = (Intro 4.4s) + (Lama Kunci misal 5s) = ~9.5s
    m_duration = 9.5f;
}
void BossLockPlayerState::Update(Boss* boss, float dt)
{
    m_timer += dt;

    Player* player = boss->GetPlayer();
    if (player)
    {
        // ====================================================
        // PHASE 1: MAGNETIC PULL (Saat Gembok Sedang Menutup)
        // ====================================================
        // Animasi menutup dimulai detik ke-4.0 dan selesai di 4.4
        if (m_timer >= 4.0f && m_timer < 4.4f)
        {
            // Lock Movement Input agar player tidak bisa lari
            player->SetMovementLock(true);

            // Logika Tarik (Lerp)
            DirectX::XMFLOAT3 currentPos = player->GetPosition();

            // Gunakan interpolation yang agak cepat (Speed 10.0f) agar sempat sampai
            float lerpSpeed = 10.0f;

            float dx = (m_lockPosition.x - currentPos.x) * lerpSpeed * dt;
            float dz = (m_lockPosition.z - currentPos.z) * lerpSpeed * dt;

            // Terapkan tarikan
            player->SetPosition(currentPos.x + dx, 0.0f, currentPos.z + dz);
        }

        // ====================================================
        // PHASE 2: HARD SNAP (Saat Gembok Sudah Putih/Locked)
        // ====================================================
        // Detik ke 4.4 adalah saat bunyi "Cklek!" dan gembok jadi putih
        else if (m_timer >= 4.4f)
        {
            // Pastikan input terkunci
            player->SetMovementLock(true);

            // SNAP! Paksa posisi player tepat di titik lock.
            // Kita set setiap frame untuk mencegah physics/collision menggeser player.
            player->SetPosition(m_lockPosition.x, 0.0f, m_lockPosition.z);
        }
    }

    // ====================================================
    // STATE FINISH
    // ====================================================
    if (m_timer >= m_duration)
    {
        boss->AddTerminalLog("GRAVITY WELL: DISSIPATED");
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossLockPlayerState::Exit(Boss* boss)
{
    // 4. PENTING: Lepaskan Kunci saat keluar state!
    if (boss->GetPlayer())
    {
        boss->GetPlayer()->SetMovementLock(false);
        boss->AddTerminalLog("MOBILITY SYSTEMS: RESTORED");
    }

    // Perintahkan monitor untuk animasi membuka gembok
    boss->GetMonitor1()->ResetToIdle();
}

// ==========================================
// STATE: BOSS COMMAND (TYPING ANIMATION)
// ==========================================

void BossCommandState::Enter(Boss* boss)
{
    // 1. Panggil Monitor 1 untuk mulai animasi
    // (Asumsi Boss punya getter: boss->GetMonitor1())
    boss->GetMonitor1()->PlayCommandAnimation(m_message);

    m_waitTimer = 0.0f;
    boss->AddTerminalLog("EXEC: " + m_message); // Log di terminal kecil juga
}

void BossCommandState::Update(Boss* boss, float dt)
{
    // 2. Cek apakah Monitor 1 masih sibuk ngetik?
    if (boss->GetMonitor1()->IsBusy())
    {
        // Masih ngetik, jangan lakukan apa-apa
        return;
    }

    // 3. Animasi selesai (State DONE)
    // Beri jeda sedikit (misal 0.5 detik) biar player sempat baca
    m_waitTimer += dt;
    if (m_waitTimer > 0.5f)
    {
        // 4. TRANSISI KE STATE ASLI
        // Kita gunakan trik: Ambil pointer m_nextState, set member ke null (biar destructor ga hapus),
        // lalu oper ke BossStateMachine.

        BossState* next = m_nextState;
        m_nextState = nullptr; // Lepas kepemilikan

        // Pindah state!
        boss->GetStateMachine()->ChangeState(boss, next);
    }
}

void BossCommandState::Exit(Boss* boss)
{
    // Saat keluar, Monitor 1 akan otomatis kembali ke Idle di Update loop-nya sendiri
    // atau kita bisa paksa reset status animasinya jika perlu.
    // Tapi logic di TerminalMonitor1::Update bagian IDLE sudah handle lerp balik.
}

// ==========================================
// STATE: DOWNLOAD ATTACK
// ==========================================

void BossDownloadAttackState::Enter(Boss* boss)
{
    // 1. Visual & Audio Cues
    boss->GetMonitor1()->PlayCommandAnimation("DOWNLOADING FILES...");
    boss->AddTerminalLog("PROTOCOL: DATA_SIPHON");
    boss->AddTerminalLog("SOURCE: EXTERNAL_SERVER");

    // 2. Setup Awal
    m_timer = 0.0f;
    m_spawnTimer = 0.0f;

    // 3. Pastikan part "antenna" ada dan reset posisinya
    if (boss->HasPart("antenna"))
    {
        BossPart* antenna = boss->GetPart("antenna");
        antenna->position = m_antennaHiddenPos;
        antenna->useFloating = false; // Matikan floating otomatis agar kita bisa kontrol manual
    }
}

void BossDownloadAttackState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    BossPart* antenna = boss->GetPart("antenna");

    float safeDt = dt;
    if (safeDt > 0.05f) safeDt = 0.05f;

    // =========================================================
    // PHASE 1: SLIDE IN (Masuk Layar)
    // =========================================================
    if (m_timer < m_durationSlideIn)
    {
        if (antenna)
        {
            // Hitung progress 0.0 - 1.0
            float t = m_timer / m_durationSlideIn;

            // Easing: SmoothStep (t^2 * (3 - 2t)) biar gerakannya luwes
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Position
            float x = m_antennaHiddenPos.x + (m_antennaActivePos.x - m_antennaHiddenPos.x) * smoothT;
            float y = m_antennaHiddenPos.y + (m_antennaActivePos.y - m_antennaHiddenPos.y) * smoothT;
            float z = m_antennaHiddenPos.z + (m_antennaActivePos.z - m_antennaHiddenPos.z) * smoothT;

            antenna->position = { x, y, z };
        }
    }
    // =========================================================
    // PHASE 2: ATTACK (Spawn Files)
    // =========================================================
    else if (m_timer < (m_durationSlideIn + m_durationAttack))
    {
        // Pastikan antena diam di posisi aktif
        if (antenna) antenna->position = m_antennaActivePos;

        // Logic Spawn
        m_spawnTimer += dt;
        if (m_spawnTimer > 0.08f) // Spawn sangat cepat (setiap 0.08 detik)
        {
            m_spawnTimer = 0.0f;

            // Target: Ujung atas antena
            DirectX::XMFLOAT3 target = m_antennaActivePos;
            target.y += 4.0f; // Offset ke ujung tiang
            target.x -= 0.5f;

            // Variasi posisi spawn sedikit biar tidak satu garis lurus
            DirectX::XMFLOAT3 spawnPos = m_fileSpawnSource;
            spawnPos.x += ((rand() % 100) / 50.0f) - 1.0f; // Random offset -1 s/d 1
            spawnPos.y += ((rand() % 100) / 50.0f) - 1.0f;

            boss->SpawnFileProjectile(spawnPos, target);
        }
    }
    // =========================================================
    // PHASE 3: SLIDE OUT (Keluar Layar)
    // =========================================================
    else if (m_timer < (m_durationSlideIn + m_durationAttack + m_durationSlideOut))
    {
        if (antenna)
        {
            // Hitung waktu lokal untuk fase keluar
            float exitTime = m_timer - (m_durationSlideIn + m_durationAttack);
            float t = exitTime / m_durationSlideOut;
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Kebalik: Active -> Hidden
            float x = m_antennaActivePos.x + (m_antennaHiddenPos.x - m_antennaActivePos.x) * smoothT;
            float y = m_antennaActivePos.y + (m_antennaHiddenPos.y - m_antennaActivePos.y) * smoothT;
            float z = m_antennaActivePos.z + (m_antennaHiddenPos.z - m_antennaActivePos.z) * smoothT;

            antenna->position = { x, y, z };
        }
    }
    // =========================================================
    // FINISH
    // =========================================================
    else
    {
        boss->ChangeState(new BossIdleState());
    }

    // =========================================================
    // UPDATE LOGIC PROJECTILE
    // (Update Fisika Peluru di sini)
    // =========================================================
    auto& projectiles = boss->GetProjectiles();
    for (auto& p : projectiles)
    {
        if (!p.active) continue;

        // Gerak menuju Target (Homing linear)
        float dx = p.targetPos.x - p.position.x;
        float dy = p.targetPos.y - p.position.y;
        float dz = p.targetPos.z - p.position.z;

        float dist = sqrt(dx * dx + dy * dy + dz * dz);

        // Kecepatan frame-independent
        float step = p.speed * safeDt;

        if (dist < step) // Sampai di tujuan
        {
            p.active = false; // Matikan (Nanti bisa tambah partikel impact di sini)
        }
        else
        {
            // Normalize & Move
            p.position.x += (dx / dist) * step;
            p.position.y += (dy / dist) * step;
            p.position.z += (dz / dist) * step;

            // Rotasi Projectile (Spinning Floppy Disk)
            p.rotation.x += 360.0f * dt;
            p.rotation.y += 180.0f * dt;
        }
    }
}

void BossDownloadAttackState::Exit(Boss* boss)
{
    boss->AddTerminalLog("DOWNLOAD: COMPLETE (100%)");

    // Bersihkan semua sisa file agar tidak melayang-layang
    boss->ClearProjectiles();

    // Kembalikan Antena ke state aman
    if (boss->HasPart("antenna"))
    {
        BossPart* antenna = boss->GetPart("antenna");
        antenna->position = m_antennaHiddenPos;
        // Opsional: Nyalakan lagi floating jika ingin dipakai di state lain
        // antenna->useFloating = true; 
    }

    boss->GetMonitor1()->ResetToIdle();
}