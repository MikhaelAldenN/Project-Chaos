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

    // 2. [BARU] RANDOMIZE MIRROR (50% Chance)
    // 0 = Normal (Antenna Kanan, File Kiri)
    // 1 = Mirrored (Antenna Kiri, File Kanan)
    m_isMirrored = (rand() % 2 != 0);

    if (m_isMirrored) {
        boss->AddTerminalLog("CONFIG: MIRRORED_LAYOUT");
    }
    else {
        boss->AddTerminalLog("CONFIG: STANDARD_LAYOUT");
    }

    // 3. Setup Awal
    m_timer = 0.0f;
    m_spawnTimer = 0.0f;

    // 4. Reset Posisi Antena (Sesuai Mirror)
    if (boss->HasPart("antenna"))
    {
        BossPart* antenna = boss->GetPart("antenna");

        // Hitung Posisi Hidden Awal (Apakah positif atau negatif X)
        float startX = m_isMirrored ? -m_antennaHiddenPos.x : m_antennaHiddenPos.x;

        antenna->position = { startX, m_antennaHiddenPos.y, m_antennaHiddenPos.z };
        antenna->useFloating = false;
    }
}

void BossDownloadAttackState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    BossPart* antenna = boss->GetPart("antenna");

    float safeDt = dt;
    if (safeDt > 0.05f) safeDt = 0.05f;

    // --- HELPER VARIABLES (MIRROR LOGIC) ---
    // Kita buat target posisi lokal berdasarkan status mirror
    DirectX::XMFLOAT3 targetHidden = m_antennaHiddenPos;
    DirectX::XMFLOAT3 targetActive = m_antennaActivePos;
    DirectX::XMFLOAT3 targetRotation = boss->m_antennaRotation;

    if (m_isMirrored)
    {
        // Balik Posisi X (Kanan -> Kiri)
        targetHidden.x = -targetHidden.x;
        targetActive.x = -targetActive.x;

        // Balik Rotasi (Supaya tetap menghadap tengah)
        // Y: Flip arah hadap horizontal
        // Z: Flip kemiringan (roll)
        targetRotation.x = 445.0f;
        targetRotation.y = 472.0f;
        targetRotation.z = 101.0f;
    }

    // =========================================================
    // PHASE 1: SLIDE IN
    // =========================================================
    if (m_timer < m_durationSlideIn)
    {
        if (antenna)
        {
            float t = m_timer / m_durationSlideIn;
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Posisi (Pakai target yang sudah dimirror)
            float x = targetHidden.x + (targetActive.x - targetHidden.x) * smoothT;
            float y = targetHidden.y + (targetActive.y - targetHidden.y) * smoothT;
            float z = targetHidden.z + (targetActive.z - targetHidden.z) * smoothT;

            antenna->position = { x, y, z };

            // [BARU] Update Rotasi juga sesuai mirror
            antenna->rotation = targetRotation;
        }
    }
    // =========================================================
    // PHASE 2: ATTACK (Spawn Files)
    // =========================================================
    else if (m_timer < (m_durationSlideIn + m_durationAttack))
    {
        if (antenna) {
            antenna->position = targetActive; // Kunci di posisi aktif
            antenna->rotation = targetRotation;
        }

        m_spawnTimer += dt;
        if (m_spawnTimer > 0.08f)
        {
            m_spawnTimer = 0.0f;

            // 1. TENTUKAN TARGET (ANTENNA TIP)
            DirectX::XMFLOAT3 target = targetActive; // Sudah dimirror di atas
            target.y += 4.0f;

            // Offset X sedikit biar pas di tengah piringan
            // Kalau mirror, offsetnya dibalik juga
            float tipOffset = -0.5f;
            if (m_isMirrored) tipOffset = 0.5f;
            target.x += tipOffset;

            // 2. TENTUKAN SPAWN SOURCE (MIRRORED)
            DirectX::XMFLOAT3 spawnPos = boss->m_fileSpawnSource;

            // [PENTING] Jika mode mirror (Antenna di Kiri), Spawn harus dari Kanan
            // Default Spawn Source X adalah -19 (Kiri).
            // Jadi:
            // - Normal: Antenna Kanan (Pos), Spawn Kiri (Neg)
            // - Mirror: Antenna Kiri (Neg), Spawn Kanan (Pos)
            if (m_isMirrored) spawnPos.x = -spawnPos.x;

            // Variasi Z (Spread)
            float spreadZ = 8.0f;
            float randomFactor = ((rand() % 200) / 100.0f) - 1.0f;
            spawnPos.z += (randomFactor * spreadZ);

            // Variasi X sedikit
            spawnPos.x += ((rand() % 100) / 50.0f) - 1.0f;

            // 3. SPAWN
            boss->SpawnFileProjectile(spawnPos, target);
        }
    }
    // =========================================================
    // PHASE 3: SLIDE OUT
    // =========================================================
    else if (m_timer < (m_durationSlideIn + m_durationAttack + m_durationSlideOut))
    {
        if (antenna)
        {
            float exitTime = m_timer - (m_durationSlideIn + m_durationAttack);
            float t = exitTime / m_durationSlideOut;
            float smoothT = t * t * (3.0f - 2.0f * t);

            // Lerp Kebalik: Active -> Hidden
            float x = targetActive.x + (targetHidden.x - targetActive.x) * smoothT;
            float y = targetActive.y + (targetHidden.y - targetActive.y) * smoothT;
            float z = targetActive.z + (targetHidden.z - targetActive.z) * smoothT;

            antenna->position = { x, y, z };
            antenna->rotation = targetRotation;
        }
    }
    else
    {
        boss->ChangeState(new BossIdleState());
    }

    // =========================================================
    // UPDATE LOGIC PROJECTILE (SAMA SEPERTI SEBELUMNYA)
    // =========================================================
    auto& projectiles = boss->GetProjectiles();
    for (auto& p : projectiles)
    {
        if (!p.active) continue;

        float dx = p.targetPos.x - p.position.x;
        float dy = p.targetPos.y - p.position.y;
        float dz = p.targetPos.z - p.position.z;

        float dist = sqrt(dx * dx + dy * dy + dz * dz);
        float step = p.speed * safeDt;

        if (dist < step) {
            p.active = false;
        }
        else {
            p.position.x += (dx / dist) * step;
            p.position.y += (dy / dist) * step;
            p.position.z += (dz / dist) * step;

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

// ==========================================
// STATE: SPAWN PENTAGON (RADIAL BURST)
// ==========================================
void BossSpawnPentagonState::Enter(Boss* boss)
{
    boss->AddTerminalLog("WARNING: HEAVY UNIT DETECTED");
    boss->AddTerminalLog("PROTOCOL: RADIAL_BURST");

    EnemyManager* em = boss->GetEnemyManager();
    if (em)
    {
        EnemySpawnConfig config;

        // 1. POSISI (Ambil dari Slider Boss)
        config.Position = boss->m_pentagonPos;

        config.Rotation = { 0.0f, 0.0f, 0.0f };
        config.Color = { 1.0f, 0.0f, 1.0f, 1.0f };
        config.Type = EnemyType::Pentagon;
        config.AttackBehavior = AttackType::RadialBurst;

        // 2. [FIX] STOP GERAKAN
        // Ubah dari MoveDir::Right menjadi None
        config.Direction = MoveDir::None;

        // Reset range patroli biar aman (gak ngaruh kalau None, tapi biar rapi)
        config.MinX = 0.0f;
        config.MaxX = 0.0f;

        // 3. [FIX] SCALE (Ambil dari Slider Boss)
        float s = boss->m_pentagonScale;
        config.Scale = { s, s, s };

        em->SpawnEnemy(config);

        boss->AddTerminalLog("UNIT DEPLOYED: PENTAGON (STATIC)");
    }

    m_timer = 0.0f;
}

void BossSpawnPentagonState::Update(Boss* boss, float dt)
{
    m_timer += dt;
    // Tunggu durasi selesai, lalu kembali Idle
    if (m_timer >= m_duration)
    {
        boss->GetStateMachine()->ChangeState(boss, new BossIdleState());
    }
}

void BossSpawnPentagonState::Exit(Boss* boss)
{
    // Cleanup jika perlu
}