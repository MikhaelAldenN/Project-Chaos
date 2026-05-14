#include "NaviPhaseNormal.h"
#include "NaviBoss.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include <SDL3/SDL.h>

#include "System/Input.h"
#include "Player.h"
#include "StateMachine.h"
#include "PlayerStates.h"

#include "WindowTrackingSystem.h"

#include <random> // Pastikan ini ada di atas!
#include <CameraController.h>

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();

        // =========================================================
        // [FIX 1] LUCUTI KASTA MAIN WINDOW!
        // Set Priority ke 50 (Rakyat Biasa) agar WindowManager 
        // membuangnya ke HWND_NOTOPMOST!
        // =========================================================
        mainWindow->SetPriority(50);

        SDL_SetWindowAlwaysOnTop(sdlWin, false);
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, 0, 0);

        // =========================================================
        // [FIX 2] HACK "FULLSCREEN OPTIMIZATION" WINDOWS OS!
        // Tambahkan +1 pixel pada tinggi (Height) layar.
        // Mata pemain tidak akan menyadari 1 pixel ini yang tersembunyi di bawah taskbar,
        // TAPI Windows OS akan mengira ini bukan "Exclusive Fullscreen" dan 
        // akan mematuhi aturan Z-Order kita!
        // =========================================================
        SDL_SetWindowSize(sdlWin, screenW, screenH + 1);
    }

    if (boss->GetMainWindow()) {
        // Navi Boss tetap di Kasta Dewa (0)
        boss->GetMainWindow()->SetPriority(0);

        // Memicu WindowManager untuk menata ulang Z-Order
        WindowManager::Instance().MarkPriorityDirty();
    }

    // Matikan Breathing (Intensity = 0.0f) agar Navi statik
    boss->SetCoreBreathParams(1.0f, 0.0f);

    m_bulletPool.clear();
    m_bulletPool.reserve(200);
    for (int i = 0; i < 200; ++i) {
        auto b = std::make_unique<Bullet>();
        b->SetActive(false);
        m_bulletPool.push_back(std::move(b));
    }

    m_zonePrimitive = std::make_unique<Primitive>(Graphics::Instance().GetDevice());
}

void NaviPhaseNormal::TriggerTripleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true;
        m_burstsFired = 0;
        m_burstSequenceTimer = 0.0f;
    }
}

// =========================================================
// [NEW] LOGIKA PENGUNCIAN TARGET (LOCK-ON)
// =========================================================
void NaviPhaseNormal::TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos) {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiringFan = true;
        m_fanWavesFired = 0;
        m_fanTimer = m_params.fanWaveDelay; // Dibuat penuh agar langsung menembak di frame 1

        DirectX::XMFLOAT3 bossPos = boss->GetPosition();
        float dx = playerPos.x - bossPos.x;
        float dz = playerPos.z - bossPos.z;

        // Kunci sudut arah player pada saat tombol ditekan
        m_lockedBaseAngle = atan2f(dx, dz);
    }
}

void NaviPhaseNormal::Update(float dt, NaviBoss* boss) {
    if (!boss) return;
    // =========================================================
    // --- 0. JALANKAN OTAK AI DIRECTOR ---
    // =========================================================
    UpdateAI(dt, boss);

    // =========================================================
    // --- 0A. LOGIKA PERGERAKAN JENDELA (LERP) ---
    // =========================================================
// 1. EASE-IN: Lerp kecepatannya dari 0 menuju Target Kecepatan
    m_currentMoveLerpSpeed += (m_moveLerpSpeed - m_currentMoveLerpSpeed) * m_moveAcceleration * dt;

    // 2. EASE-OUT: Lerp posisinya menggunakan kecepatan yang sudah dihaluskan
    DirectX::XMFLOAT3 currentPos = boss->GetPosition();
    currentPos.x += (m_targetPosition.x - currentPos.x) * m_currentMoveLerpSpeed * dt;
    currentPos.z += (m_targetPosition.z - currentPos.z) * m_currentMoveLerpSpeed * dt;
    
    boss->SetPosition(currentPos);

    // =========================================================
    // SINKRONISASI UKURAN JENDELA DENGAN ZOOM CAMERA
    // =========================================================
    auto* ws = boss->GetWindowSystem();
    if (ws) {
        float currentP2U = ws->GetPixelToUnitRatio();
        
        // [FIX 5] Ukuran 6.0f unit biasanya lebih pas untuk komposisi wajah Navi
        // saat kamera sedang melakukan zoom-in.
        float newSize = 5.0f * currentP2U; 
        
        boss->SetBaseWindowSize(newSize, newSize);
        boss->SetWindowSize(newSize, newSize);
    }

    // --- 1A. Logika Radial Burst (TRIPLE BURST) ---
    if (m_isFiring) {
        // Tembakan Pertama (Instan)
        if (m_burstsFired == 0) {
            FireRadialBurst(boss, 0.0f);
            m_burstsFired++;
        }
        // Tembakan Kedua dan Ketiga (Dengan Delay)
        else if (m_burstsFired < 3) {
            m_burstSequenceTimer += dt;
            if (m_burstSequenceTimer >= m_params.burstDelay) {
                m_burstSequenceTimer = 0.0f; // Reset timer!

                // [JUICE] Selang-seling offset rotasi agar membentuk jaring maut!
                float step = DirectX::XM_2PI / (float)m_params.count;
                // Tembakan ke-2 (index 1) di-offset. Tembakan ke-3 (index 2) kembali lurus.
                float offset = (m_burstsFired % 2 == 1) ? (step * 0.5f) : 0.0f;

                FireRadialBurst(boss, offset);
                m_burstsFired++;

                if (m_burstsFired >= 3) {
                    m_isFiring = false; // Matikan serangan setelah 3 gelombang
                }
            }
        }
    }

    // --- 1B. Logika Targeted Fan Burst ---
    if (m_isFiringFan) {
        m_fanTimer += dt;
        if (m_fanTimer >= m_params.fanWaveDelay) {
            m_fanTimer = 0.0f;
            FireFanWave(boss); // Muntahkan 1 baris peluru
            m_fanWavesFired++;

            if (m_fanWavesFired >= m_params.fanWaves) {
                m_isFiringFan = false; // Berhenti jika sudah 5x
            }
        }
    }

    // --- 1C. Logika Glintstone Phalanx ---
    if (m_phalanxState > 0) {
        m_phalanxTimer += dt;
        DirectX::XMFLOAT3 bPos = boss->GetPosition();

        // 1. BUAT PELURU MELAYANG (HOVERING) MENGIKUTI ARAH PLAYER
        for (int i = m_phalanxFired; i < m_phalanxSpawned; ++i) {
            if (m_phalanxBullets[i] && m_phalanxBullets[i]->IsActive()) {

                // --- A. HITUNG TARGET (Sama seperti sebelumnya) ---
                DirectX::XMFLOAT3 pPos = m_phalanxTarget->GetPosition();
                float dx = pPos.x - bPos.x;
                float dz = pPos.z - bPos.z;
                float angleToPlayer = atan2f(dx, dz);

                float totalSpread = DirectX::XM_PI;
                float startOffset = -totalSpread * 0.5f;
                float step = m_params.phalanxCount > 1 ? totalSpread / (float)(m_params.phalanxCount - 1) : 0.0f;
                float finalBulletAngle = angleToPlayer + (startOffset + (i * step));

                DirectX::XMFLOAT3 targetHoverPos = bPos;
                targetHoverPos.x += sinf(finalBulletAngle) * m_params.phalanxHoverRadius;
                targetHoverPos.z += cosf(finalBulletAngle) * m_params.phalanxHoverRadius;
                targetHoverPos.y += 1.0f;

                // =========================================================
                // [FIX MUTLAK] EASE OUT / SMOOTHING POSITION
                // =========================================================
                // Ambil posisi peluru saat ini
                DirectX::XMFLOAT3 currentPos = m_phalanxBullets[i]->GetMovement()->GetPosition();

                // Rumus Lerp: current + (target - current) * speed * dt
                // Ini akan menciptakan efek melambat saat mendekati target (Ease Out)
                float sSpeed = m_params.phalanxSmoothSpeed;
                currentPos.x += (targetHoverPos.x - currentPos.x) * sSpeed * dt;
                currentPos.y += (targetHoverPos.y - currentPos.y) * sSpeed * dt;
                currentPos.z += (targetHoverPos.z - currentPos.z) * sSpeed * dt;

                // Terapkan posisi yang sudah dihaluskan
                m_phalanxBullets[i]->GetMovement()->SetPosition(currentPos);

                // Rotasi tetap nge-aim player agar terlihat mengancam
                m_phalanxBullets[i]->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(angleToPlayer));
            }
        }

        // 2. FASE 1: CHARGING / MUNCUL SATU PER SATU
        if (m_phalanxState == 1) {
            if (m_phalanxTimer >= m_params.phalanxChargeDelay) {
                m_phalanxTimer -= m_params.phalanxChargeDelay;

                for (auto& bullet : m_bulletPool) {
                    if (!bullet->IsActive()) {
                        bullet->SetActive(true);
                        // MANDIKAN PELURU
                        bullet->ApplyMovement(bPos, { 0,0,0 });
                        bullet->SetHomingTarget(nullptr);
                        bullet->SetBossTarget(nullptr);
                        bullet->SetParabolic(false);
                        bullet->SetParryReturn(false);

                        // SETUP KHUSUS PHALANX
                        bullet->SetRadius(0.35f);
                        bullet->scale = { 2.0f, 2.0f, 2.0f }; // Agak besar, ancaman nyata
                        bullet->SetTurnSpeed(m_params.phalanxTurnSpeed); // Buat dia BODOH saat membelok!

                        m_phalanxBullets.push_back(bullet.get());
                        m_phalanxSpawned++;
                        break;
                    }
                }

                // Jika sudah full 5 peluru, ganti state jadi Firing!
                if (m_phalanxSpawned >= m_params.phalanxCount) {
                    m_phalanxState = 2;
                    m_phalanxTimer = 0.0f; // Reset timer untuk delay tembakan
                }
            }
        }

        // FASE 2: HOLDING (Diam dalam formasi penuh)
        else if (m_phalanxState == 2) {
            if (m_phalanxTimer >= m_params.phalanxHoldDuration) {
                m_phalanxState = 3; // Lanjut ke menembak
                m_phalanxTimer = 0.0f;
            }
        }

        // 3. FASE 2: FIRING / TEMBAK SATU PER SATU
        else if (m_phalanxState == 3) {
            if (m_phalanxTimer >= m_params.phalanxFireDelay) {
                m_phalanxTimer -= m_params.phalanxFireDelay;

                if (m_phalanxFired < m_phalanxSpawned) {
                    Bullet* b = m_phalanxBullets[m_phalanxFired];
                    if (b && b->IsActive()) {
                        b->SetHomingTarget(m_phalanxTarget); // Kunci ke Player!

                        // Arah tembakan awal (Lurus ke player)
                        DirectX::XMFLOAT3 myPos = b->GetMovement()->GetPosition();
                        DirectX::XMFLOAT3 pPos = m_phalanxTarget->GetPosition();
                        float dx = pPos.x - myPos.x;
                        float dz = pPos.z - myPos.z;
                        float dist = std::sqrt(dx * dx + dz * dz);

                        DirectX::XMFLOAT3 dir = { 0,0,1 };
                        if (dist > 0.001f) dir = { dx / dist, 0.0f, dz / dist };

                        b->Fire(myPos, dir, m_params.phalanxSpeed);
                    }
                    m_phalanxFired++;
                }

                // Jika semua sudah tertembak, matikan serangan
                if (m_phalanxFired >= m_phalanxSpawned) {
                    m_phalanxState = 4;
                    m_phalanxTimer = 0.0f;
                    m_phalanxBullets.clear();
                }
            }
        }

        // [BARU] FASE 4: POST-FIRE DELAY (Diam di sisi sebentar)
        else if (m_phalanxState == 4) {
            if (m_phalanxTimer >= m_params.phalanxPostFireDelay) {
                // Selesai jeda, baru tentukan target balik ke tengah
                m_phalanxState = 5;
                m_targetPosition = { 0.0f, 0.0f, 0.0f };
                m_moveLerpSpeed = m_params.phalanxReturnMoveSpeed; // Speed diperlambat
                m_currentMoveLerpSpeed = 0.0f;
            }
        }

        // [BARU] FASE 5: RETURNING (Menunggu sampai benar-benar di tengah)
        else if (m_phalanxState == 5) {
            DirectX::XMFLOAT3 pos = boss->GetPosition();
            // Cek jarak kuadrat ke tengah (0,0,0)
            float distSq = pos.x * pos.x + pos.z * pos.z;
            if (distSq < 0.01f) {
                m_phalanxState = 0; // Benar-benar selesai, AI boleh nyerang lagi
                m_phalanxBullets.clear();
            }
        }
    }

    // --- 1D. Logika Asgore Rain (Area Denial) ---
    if (m_rainState > 0) {
        m_rainTimer += dt;

        // =========================================================
        // [FIX MUTLAK] GETARAN PERSISTEN
        // Kocok layar terus-menerus selama Fase 2 (Hujan Deras) DAN Fase 3 (Reda)!
        // =========================================================
        if (m_rainState == 2 || m_rainState == 3) {
            CameraController::Instance().AddTrauma(1.0f * dt); // Agak dinaikkan biar makin terasa
        }

        if (m_rainState == 1) { // FASE WARNING
            if (m_rainTimer >= m_params.rainWarningDuration) {
                m_rainState = 2; // Mulai Hujan! (Hitbox Aktif)
                m_rainTimer = 0.0f;
                CameraController::Instance().AddTrauma(0.5f); // Ledakan getaran awal
            }
        }
        else if (m_rainState == 2) { // FASE RAINING (Mematikan)
            if (m_rainTimer >= m_params.rainActiveDuration) {
                m_rainState = 3;
                m_rainTimer = 0.0f;
            }
        }
        else if (m_rainState == 3) { // FASE DISSIPATING (Reda & Aman)
            // Tunggu 1.5 detik agar peluru terakhir benar-benar keluar layar bawah
            if (m_rainTimer >= 1.0f) {
                m_rainState = 0; // Hujan benar-benar bersih
            }
        }
    }

    if (m_isLaserLocked && m_laserTargetPlayer) {
        m_laserTimer += dt;
        DirectX::XMFLOAT3 pPos = m_laserTargetPlayer->GetPosition();
        DirectX::XMFLOAT3 bPos = boss->GetPosition();

        // --- 1. UPDATE FISIK BOLA BIJUUDAMA ---
        if (m_bijuudamaBall && m_bijuudamaBall->IsActive()) {
            DirectX::XMFLOAT3 vel = m_bijuudamaBall->GetVelocity();
            if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {

                // [TIDAK HARDCODE] Tempel bola di depan mulut Boss
                DirectX::XMFLOAT3 offsetPos = bPos;
                offsetPos.z -= m_params.bijuudamaSpawnOffsetZ;
                m_bijuudamaBall->GetMovement()->SetPosition(offsetPos);

                // [TIDAK HARDCODE] Kalkulasi Hitbox vs Visual
                float progress = min(1.0f, m_laserTimer / m_params.laserDuration);

                float currentHitboxRadius = m_params.bijuudamaBaseHitbox + (m_params.bijuudamaMaxHitboxGrow * progress);
                m_bijuudamaBall->SetRadius(currentHitboxRadius);

                float visualScale = currentHitboxRadius * m_params.bijuudamaVisualMultiplier;
                m_bijuudamaBall->scale = { visualScale, visualScale, visualScale };
            }
        }

        // --- 2. VISUAL TIMING RING (Tetap ada di Player) ---
        auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
        float t = m_laserTimer / m_params.laserDuration;
        if (t > 1.0f) t = 1.0f;
        float currentRingRadius = m_params.laserStartRadius + (m_params.laserTargetRadius - m_params.laserStartRadius) * t;

        DirectX::XMFLOAT4 ringColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // Merah (Belum pas)
        float timeDiff = std::abs(m_laserTimer - m_params.laserDuration);
        if (timeDiff <= m_params.laserParryWindow) {
            ringColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Putih Menyala! (PARRY SEKARANG!)
        }
        shapeRenderer->DrawSphere(pPos, currentRingRadius, ringColor);

        // --- 3. RESOLUSI: WAKTU CHARGE HABIS ---
        if (m_laserTimer >= m_params.laserDuration) {
            m_isLaserLocked = false;

            m_isBijuudamaRecovering = true;
            m_bijuudamaRecoveryTimer = 0.0f;

            if (m_bijuudamaBall && m_bijuudamaBall->IsActive()) {
                DirectX::XMFLOAT3 vel = m_bijuudamaBall->GetVelocity();
                if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {

                    float dx = pPos.x - bPos.x;
                    float dz = pPos.z - bPos.z;
                    float dist = std::sqrt(dx * dx + dz * dz);

                    if (dist > 0.001f) {
                        // [TIDAK HARDCODE] Kecepatan tembak
                        float sSpeed = m_params.bijuudamaShootSpeed;
                        DirectX::XMFLOAT3 shootVel = { (dx / dist) * sSpeed, 0.0f, (dz / dist) * sSpeed };
                        DirectX::XMFLOAT3 currentPos = m_bijuudamaBall->GetMovement()->GetPosition();

                        m_bijuudamaBall->ApplyMovement(currentPos, shootVel);
                    }
                }
            }
            m_bijuudamaBall = nullptr;
            m_laserTimer = 0.0f;
        }
    }

    // =========================================================
// --- 4. RECOVERY BIJUUDAMA (DIAM DI ATAS SEJENAK) ---
// =========================================================
    if (m_isBijuudamaRecovering) {
        m_bijuudamaRecoveryTimer += dt;

        if (m_bijuudamaRecoveryTimer >= m_params.bijuudamaPostFireDelay) {
            // Waktu napas habis, suruh bos pulang ke tengah dengan lambat
            m_isBijuudamaRecovering = false;
            m_targetPosition = { 0.0f, 0.0f, 0.0f };
            m_moveLerpSpeed = m_params.bijuudamaReturnMoveSpeed;
            m_currentMoveLerpSpeed = 0.0f;
        }
    }

// --- 2. Update & Recycle (DYNAMIC SCREEN BOUNDS) ---
    float limitX = 30.0f; // Fallback
    float limitZ = 20.0f; // Fallback

    if (ws) {
        // Ambil rasio piksel terbaru yang sudah dipengaruhi Zoom dari SceneBoss
        float p2u = ws->GetPixelToUnitRatio();
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        // Hitung batas dunia berdasarkan resolusi layar OS saat ini
        // Ditambah padding agar peluru tidak "pop-out" secara kasar
        limitX = ((screenW / 2.0f) / p2u) + m_params.screenDespawnPadding;
        limitZ = ((screenH / 2.0f) / p2u) + m_params.screenDespawnPadding;
    }

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) continue;

        // Jalankan pergerakan peluru
        bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

        // [OPTIMISASI MUTLAK] 
        // Jika peluru berada di luar kotak layar, langsung balikin ke Pool!
        // Ini akan otomatis menghentikan Phalanx yang mencoba memutar balik dari luar layar.
        if (bPos.x < -limitX || bPos.x > limitX || bPos.z < -limitZ || bPos.z > limitZ) {
            bullet->SetActive(false);
        }
    }
}

// =========================================================
// [NEW] MATEMATIKA FAN / SHOTGUN SPREAD
// =========================================================
void NaviPhaseNormal::FireFanWave(NaviBoss* boss) {
    int lines = m_params.fanLines;
    float spread = m_params.fanSpreadAngle;

    // Cari sudut paling ujung kiri (Start Angle) agar seimbang di tengah
    float startAngle = m_lockedBaseAngle - ((lines - 1) * spread / 2.0f);

    int firedCount = 0;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {

            // =========================================================
            // [FIX] MANDIKAN PELURU SEBELUM JADI PELURU NORMAL!
            // =========================================================
            bullet->SetRadius(0.25f);
            bullet->scale = { 1.0f, 1.0f, 1.0f };
            bullet->SetHomingTarget(nullptr);
            bullet->SetBossTarget(nullptr);
            bullet->SetParabolic(false);
            bullet->SetTurnSpeed(8.0f);

            float currentAngle = startAngle + (firedCount * spread);
            DirectX::XMFLOAT3 dir = { sinf(currentAngle), 0.0f, cosf(currentAngle) };

            bullet->Fire(boss->GetPosition(), dir, m_params.speed);

            firedCount++;
            if (firedCount >= lines) break; // Berhenti jika sudah menembak 3-4 peluru
        }
    }
}

void NaviPhaseNormal::FireRadialBurst(NaviBoss* boss, float angleOffset) {
    int firedCount = 0;
    float angleStep = DirectX::XM_2PI / (float)m_params.count;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {

            // =========================================================
            // [FIX] MANDIKAN PELURU SEBELUM JADI PELURU NORMAL!
            // =========================================================
            bullet->SetRadius(0.25f);                     // Hitbox standar peluru kecil
            bullet->scale = { 1.0f, 1.0f, 1.0f };         // Skala visual standar
            bullet->SetHomingTarget(nullptr);             // Hapus pelacak player
            bullet->SetBossTarget(nullptr);               // Hapus pelacak bos (Shatter)
            bullet->SetParabolic(false);                  // Matikan mode melengkung
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);

            float angle = (firedCount * angleStep) + angleOffset;
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.speed);
            if (++firedCount >= m_params.count) break;
        }
    }
}


void NaviPhaseNormal::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera) return;
    auto renderer = Graphics::Instance().GetModelRenderer();
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();

    for (auto& bullet : m_bulletPool) {
        if (bullet->IsActive()) {
            DirectX::XMFLOAT3 vel = bullet->GetVelocity();
            float speedSq = vel.x * vel.x + vel.z * vel.z;

            DirectX::XMFLOAT4 renderColor = m_params.color;

            if (bullet.get() == m_bijuudamaBall || bullet->GetBossTarget() != nullptr) {
                renderColor = m_params.bijuudamaColor; // Tetap Merah!
            }
            // Jika peluru biasa yang dipantulkan (Kecepatan tinggi), baru ubah jadi Cyan
            else if (speedSq > 900.0f) {
                renderColor = { 1.0f, 0.0f, 0.0f, 1.0f };
            }

            renderer->Draw(ShaderId::Phong, bullet->GetModel(), renderColor);
        }
    }

    // =========================================================
    // [NEW] RENDER VISUAL RITME LASER (BOLA MENYUSUT)
    // =========================================================
    if (m_isLaserLocked && m_laserTargetPlayer) {
        DirectX::XMFLOAT3 pPos = m_laserTargetPlayer->GetPosition();
        pPos.y += 1.0f; // Setinggi dada player

        // A. Gambar Bola Target (Kecil, Cyan)
        shapeRenderer->DrawSphere(pPos, m_params.laserTargetRadius, { 0.0f, 1.0f, 1.0f, 1.0f });

        // B. Hitung ukuran Bola Menyusut (Interpolasi / Lerp manual)
        float t = m_laserTimer / m_params.laserDuration;
        if (t > 1.0f) t = 1.0f;
        float currentRadius = m_params.laserStartRadius + (m_params.laserTargetRadius - m_params.laserStartRadius) * t;

        // C. Visual Feedback Timing (Bola menyusut berubah putih jika masuk zona Parry!)
        DirectX::XMFLOAT4 shrinkColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // Merah (Belum pas)
        float timeDiff = std::abs(m_laserTimer - m_params.laserDuration);
        if (timeDiff <= m_params.laserParryWindow) {
            shrinkColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Putih Menyala! (TEKAN SPACE SEKARANG!)
        }

        shapeRenderer->DrawSphere(pPos, currentRadius, shrinkColor);
    }
    // =========================================================
    // [NEW] RENDER ASGORE RAIN ZONE & PROCEDURAL BULLETS
    // =========================================================
    if (m_rainState > 0) {
        float actualW = GetActualRainWidth();
        float actualD = GetActualRainDepth();
        float halfW = actualW * 0.5f;
        float halfD = actualD * 0.5f;

        // 1. GAMBAR WARNING ZONE 2D (HANYA DI FASE 1 / SEBELUM HUJAN)
        if (m_rainState == 1) {
            float blink = (sinf(m_rainTimer * 20.0f) + 1.0f) * 0.5f;
            float alpha = 0.2f + (blink * 0.4f);

            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            DirectX::XMFLOAT3 camPos = currentCamera->GetPosition();

            float width2D = actualW * p2u;
            float height2D = actualD * p2u;

            // Fungsi mini (Lambda) untuk menggambar kotak merah
            auto drawWarningRect = [&](DirectX::XMFLOAT3 centerPos) {
                float screenX = (centerPos.x - camPos.x) * p2u + (screenW / 2.0f);
                float screenY = -(centerPos.z - camPos.z) * p2u + (screenH / 2.0f);
                m_zonePrimitive->Rect(screenX, screenY, width2D, height2D, width2D * 0.5f, height2D * 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, alpha);
                };

            // Gambar kotak utama (Kiri)
            drawWarningRect(m_rainCenter);
            // Jika mode Dual aktif, gambar juga kotak kedua (Kanan)!
            if (m_rainIsDual) drawWarningRect(m_rainCenter2);

            m_zonePrimitive->Render(context);
        }

        // 2. ILUSI HUJAN PELURU ORGANIK (HANYA DI FASE 2 DERAS & FASE 3 REDA)
        if (m_rainState == 2 || m_rainState == 3) {
            std::mt19937 gen(1337);
            std::uniform_real_distribution<float> distSpeed(50.0f, 90.0f);
            std::uniform_real_distribution<float> distSpawn(0.0f, m_params.rainActiveDuration);

            float currentGlobalTime = (m_rainState == 2) ? m_rainTimer : (m_params.rainActiveDuration + m_rainTimer);
            int dropCount = m_rainIsDual ? 800 : 400;

            for (int i = 0; i < dropCount; ++i) {
                float speed = distSpeed(gen);
                float spawnTime = distSpawn(gen);
                float localTime = currentGlobalTime - spawnTime;

                if (localTime >= 0.0f) {

                    if (m_rainIsDual) {
                        // --- DUAL MODE (KIRI KANAN SEKALIGUS) ---
                        // Bagi rata: peluru genap jatuh di kiri, peluru ganjil jatuh di kanan
                        DirectX::XMFLOAT3 activeCenter = (i % 2 == 0) ? m_rainCenter : m_rainCenter2;
                        std::uniform_real_distribution<float> distX(activeCenter.x - halfW, activeCenter.x + halfW);

                        float rx = distX(gen);
                        float topEdge = activeCenter.z + halfD + 5.0f;
                        float bottomEdge = activeCenter.z - halfD;
                        float z = topEdge - (localTime * speed);

                        if (z >= bottomEdge) {
                            shapeRenderer->DrawSphere({ rx, 1.0f, z }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                        }
                    }
                    else if (m_rainIsVertical) {
                        // --- ZONA KIRI / KANAN (SIDE MODE) ---
                        // Kotak memanjang vertikal, jadi Hujan JATUH DARI ATAS KE BAWAH (Sumbu Z)
                        std::uniform_real_distribution<float> distX(m_rainCenter.x - halfW, m_rainCenter.x + halfW);
                        float rx = distX(gen);

                        // Mulai dari luar batas atas layar, mengalir turun (-Z)
                        float topEdge = m_rainCenter.z + halfD + 5.0f;
                        float bottomEdge = m_rainCenter.z - halfD;
                        float z = topEdge - (localTime * speed);

                        // Gambar selama belum melewati batas bawah layar
                        if (z >= bottomEdge) {
                            shapeRenderer->DrawSphere({ rx, 1.0f, z }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                        }
                    }
                    else {
                        // --- ZONA ATAS / BAWAH (NORMAL MODE) ---
                        std::uniform_real_distribution<float> distZ(m_rainCenter.z - halfD, m_rainCenter.z + halfD);
                        float rz = distZ(gen);

                        // [FIX MUTLAK] Gunakan arah sapuan dari AI!
                        float dir = m_rainSweepDir;

                        // Jika dir > 0 (Ke Kanan), mulai dari Kiri (-). 
                        // Jika dir < 0 (Ke Kiri), mulai dari Kanan (+)
                        float startX = (dir > 0) ? (m_rainCenter.x - halfW - 5.0f) : (m_rainCenter.x + halfW + 5.0f);
                        float endX = (dir > 0) ? (m_rainCenter.x + halfW) : (m_rainCenter.x - halfW);

                        float x = startX + (localTime * speed * dir);

                        // Gambar selama belum melewati batas layar seberangnya
                        bool isInside = (dir > 0) ? (x <= endX) : (x >= endX);
                        if (isInside) {
                            shapeRenderer->DrawSphere({ x, 1.0f, rz }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                        }
                    }
                }
            }
        }
    }
}

void NaviPhaseNormal::Exit(NaviBoss* boss) { m_bulletPool.clear(); }

void NaviPhaseNormal::TriggerBijuudama(Player* targetPlayer) {
    if (!m_isLaserLocked && targetPlayer) {
        m_isLaserLocked = true;
        m_laserTimer = 0.0f;
        m_laserTargetPlayer = targetPlayer;

        m_targetPosition = { 0.0f, 0.0f, 10.0f };
        m_moveLerpSpeed = m_params.bijuudamaAttackMoveSpeed;
        m_currentMoveLerpSpeed = 0.0f;

        // Cari peluru kosong untuk dijadikan Bijuudama
        for (auto& bullet : m_bulletPool) {
            if (!bullet->IsActive()) {
                bullet->SetActive(true);

                // =========================================================
                // [CRITICAL FIX] PEMBERSIHAN OBJECT POOL
                // =========================================================
                // 1. Gunakan ApplyMovement agar internal Bullet::velocity benar-benar menjadi 0
                bullet->ApplyMovement({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

                // 2. Bersihkan sisa memori State
                bullet->SetHomingTarget(nullptr);
                bullet->SetBossTarget(nullptr);
                bullet->SetParabolic(false);
                bullet->SetParryReturn(false); // <--- [FIX] WAJIB TAMBAHKAN INI!
                bullet->SetTurnSpeed(8.0f);

                // 3. Reset ukuran Hitbox dan Skala Visual 3D
                float baseHitbox = m_params.bijuudamaBaseHitbox;
                float baseVisual = baseHitbox * m_params.bijuudamaVisualMultiplier;

                bullet->SetRadius(baseHitbox);
                bullet->scale = { baseVisual, baseVisual, baseVisual };

                m_bijuudamaBall = bullet.get();
                break;
            }
        }
    }
}

void NaviPhaseNormal::ShatterBijuudama(DirectX::XMFLOAT3 parryPos, NaviBoss* boss) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> distCount(m_params.shatterMinFragments, m_params.shatterMaxFragments);
    std::uniform_real_distribution<float> distSize(m_params.shatterMinRadius, m_params.shatterMaxRadius);
    std::uniform_real_distribution<float> distDur(m_params.shatterMinDuration, m_params.shatterMaxDuration);
    // Sudut sebaran lengkungan (90 derajat ke kiri sampai 90 derajat ke kanan)
    std::uniform_real_distribution<float> distAngle(-DirectX::XM_PIDIV2, DirectX::XM_PIDIV2);

    int fragments = distCount(gen);
    int spawned = 0;

    DirectX::XMFLOAT3 bossPos = boss->GetPosition();
    float dx = bossPos.x - parryPos.x;
    float dz = bossPos.z - parryPos.z;
    float baseAngle = atan2f(dx, dz); // Sudut lurus ke arah bos

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            bullet->SetActive(true);

            // 1. Reset Internal State & Aktifkan Parabola
            bullet->ApplyMovement(parryPos, { 0,0,0 });
            bullet->SetBossTarget(boss);
            bullet->SetParabolic(true);
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);

            // 2. Set Ukuran
            float r = distSize(gen);
            bullet->SetRadius(r);
            bullet->scale = { r * 3.0f, r * 3.0f, r * 3.0f };

            // 3. Kalkulasi Titik Kontrol (Control Point Bezier)
            // Lempar titik kontrol sejauh parameter curve offset ke arah serong
            float spreadAngle = baseAngle + distAngle(gen);
            float curveDist = m_params.shatterCurveOffset;

            DirectX::XMFLOAT3 ctrlPoint = {
                parryPos.x + (sinf(spreadAngle) * curveDist),
                parryPos.y,
                parryPos.z + (cosf(spreadAngle) * curveDist)
            };

            // 4. Jalankan Parabola
            bullet->SetParabolaParams(parryPos, ctrlPoint, distDur(gen));

            spawned++;
            if (spawned >= fragments) break;
        }
    }
}

void NaviPhaseNormal::TriggerPhalanx(Player* targetPlayer) {
    if (m_phalanxState == 0 && targetPlayer) {
        m_phalanxState = 1;
        m_phalanxTimer = 0.0f;
        m_phalanxSpawned = 0;
        m_phalanxFired = 0;
        m_phalanxTarget = targetPlayer;
        m_phalanxBullets.clear();

        // =========================================================
        // [FIX] REPOSISI KE KIRI / KANAN
        // =========================================================
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 1);

        m_moveLerpSpeed = m_params.phalanxAttackMoveSpeed;
        m_currentMoveLerpSpeed = 0.0f;

        // Pindah ke X = -15.0f (Kiri) atau 15.0f (Kanan)
        m_targetPosition.x = (dist(gen) == 0) ? -15.0f : 15.0f;
        m_targetPosition.z = 0.0f; // Tetap di tengah sumbu Z
    }
}

void NaviPhaseNormal::TriggerRainAttack(bool isSideMode, bool isPositiveSide, float sweepDir, bool isDual) {
    if (m_rainState == 0) {
        m_rainState = 1;
        m_rainTimer = 0.0f;
        m_rainIsVertical = isSideMode;
        m_rainSweepDir = sweepDir;
        m_rainIsDual = isDual; // [NEW] Aktifkan mode ganda

        if (isDual) {
            // --- DUAL MODE (KIRI & KANAN SEKALIGUS) ---
            m_rainCenter.x = -16.0f;  // Pilar Kiri
            m_rainCenter.z = 0.0f;
            m_rainCenter2.x = 16.0f; // Pilar Kanan
            m_rainCenter2.z = 0.0f;
        }
        else if (!isSideMode) {
            // --- ZONA ATAS / BAWAH (NORMAL MODE) ---
            m_rainCenter.x = 0.0f;
            m_rainCenter.z = isPositiveSide ? 7.5f : -7.5f;
        }
        else {
            // --- ZONA KIRI / KANAN (SIDE MODE) ---
            m_rainCenter.x = isPositiveSide ? 12.5f : -12.5f;
            m_rainCenter.z = 0.0f;
        }
    }
}

void NaviPhaseNormal::TakeDamage(int damage) {
    if (m_bossHP <= 0) return; // Sudah mati

    m_bossHP -= damage;
    if (m_bossHP < 0) m_bossHP = 0;

    m_hitFlashTimer = 0.05f; // Timer untuk efek visual (jika ada)

    // [JUICE] Berikan getaran super mikro setiap kali peluru mengenai kaca jendela bos!
    CameraController::Instance().AddTrauma(0.1f);
}

void NaviPhaseNormal::UpdateAI(float dt, NaviBoss* boss) {
    if (!m_aiEnabled || !m_aiTarget) return;

    std::random_device rd;
    std::mt19937 gen(rd());

    // =========================================================
    // [TRACK A] SIDE RAIN (Mengincar Posisi X Player)
    // =========================================================
    if (m_cdRain > 0.0f) m_cdRain -= dt;

    if (m_cdRain <= 0.0f && !m_isLaserLocked && m_phalanxState == 0) {
        // NGECENG PLAYER: Cek player ada di kiri atau kanan
        bool isRightSide = (m_aiTarget->GetPosition().x > 0.0f);

        // [FIX] Gunakan 'true' agar yang muncul adalah hujan Kiri/Kanan
        TriggerRainAttack(true, isRightSide);

        std::uniform_real_distribution<float> distCD(8.0f, 15.0f);
        m_cdRain = distCD(gen);
    }

    // =========================================================
    // [TRACK B] DIRECT ATTACKS & COMBO
    // =========================================================
    bool isBusyMain = m_isFiring || m_isFiringFan || (m_phalanxState > 0) || m_isLaserLocked || m_isBijuudamaRecovering;

    if (!isBusyMain) {
        if (m_aiGlobalCooldown > 0.0f) {
            m_aiGlobalCooldown -= dt;
        }
        else {
            m_cdRadial -= dt;
            m_cdFan -= dt;
            m_cdPhalanx -= dt;
            m_cdBijuudama -= dt;
        }
    }

    if (!isBusyMain && m_aiGlobalCooldown <= 0.0f) {

        // Prioritas 1: Bijuudama (Dilarang bareng hujan jenis apapun)
        if (m_cdBijuudama <= 0.0f && m_rainState == 0) {
            TriggerBijuudama(m_aiTarget);

            TriggerRainAttack(true, false, 1.0f, true);

            std::uniform_real_distribution<float> distCD(15.0f, 25.0f);
            m_cdBijuudama = distCD(gen);
            m_aiGlobalCooldown = 1.0f;
        }
        // =========================================================
        // Prioritas 2: PHALANX + TOP/BOTTOM RAIN COMBO!
        // Hujan Palang (Horizontal Sweep) sekarang EKSKLUSIF di sini
        // =========================================================
        else if (m_cdPhalanx <= 0.0f && m_rainState == 0) { // Pastikan m_rainState == 0
            TriggerPhalanx(m_aiTarget);

            // 1. NGECENG PLAYER: Cek player ada di Atas atau Bawah untuk posisi palang
            bool isTopSide = (m_aiTarget->GetPosition().z > 0.0f);

            // 2. [FIX MUTLAK] SINKRONISASI POSISI BOS
            // Jika Navi meluncur ke Kanan (X > 0), hujan datang dari Kanan menyapu ke Kiri (-1.0f)
            // Jika Navi meluncur ke Kiri (X < 0), hujan datang dari Kiri menyapu ke Kanan (1.0f)
            float sweepDirection = (m_targetPosition.x > 0.0f) ? -1.0f : 1.0f;

            // 3. Panggil Hujan: Mode Normal (false), arah target, dan arah sapuan!
            TriggerRainAttack(false, isTopSide, sweepDirection);

            std::uniform_real_distribution<float> distCD(6.0f, 10.0f);
            m_cdPhalanx = distCD(gen);
            m_aiGlobalCooldown = 1.0f;
        }

        // Prioritas 3: Fan Wave (Shotgun)
        else if (m_cdFan <= 0.0f) {
            TriggerFanAttack(boss, m_aiTarget->GetPosition());
            std::uniform_real_distribution<float> distCD(4.0f, 8.0f);
            m_cdFan = distCD(gen);
            m_aiGlobalCooldown = 0.5f;
        }

        // Prioritas 4: Radial Burst (SELALU TRIPLE)
        else if (m_cdRadial <= 0.0f) {
            TriggerTripleBurst();

            std::uniform_real_distribution<float> distCD(2.0f, 5.0f);
            m_cdRadial = distCD(gen);
            m_aiGlobalCooldown = 0.5f;
        }
    }
}