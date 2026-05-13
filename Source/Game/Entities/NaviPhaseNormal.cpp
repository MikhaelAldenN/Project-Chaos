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
}

void NaviPhaseNormal::TriggerSingleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true; m_isDoubleBurst = false;
        m_burstsFired = 0; m_burstSequenceTimer = 0.0f;
    }
}

void NaviPhaseNormal::TriggerDoubleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true; m_isDoubleBurst = true;
        m_burstsFired = 0; m_burstSequenceTimer = 0.0f;
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
    // --- 1A. Logika Radial Burst ---
    if (m_isFiring) {
        if (m_burstsFired == 0) {
            FireRadialBurst(boss, 0.0f);
            m_burstsFired++;
            if (!m_isDoubleBurst) m_isFiring = false;
        }
        else if (m_isDoubleBurst && m_burstsFired == 1) {
            m_burstSequenceTimer += dt;
            if (m_burstSequenceTimer >= m_params.burstDelay) {
                float step = DirectX::XM_2PI / (float)m_params.count;
                FireRadialBurst(boss, step * 0.5f);
                m_isFiring = false;
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

        // 1. BUAT PELURU MELAYANG (HOVERING) DI BELAKANG BOS
        // Kita hanya menggeser peluru yang belum ditembakkan!
        for (int i = m_phalanxFired; i < m_phalanxSpawned; ++i) {
            if (m_phalanxBullets[i] && m_phalanxBullets[i]->IsActive()) {
                // Bentuk melengkung setengah lingkaran (PI)
                float totalSpread = DirectX::XM_PI;
                float startAngle = -totalSpread * 0.5f;
                float angleStep = m_params.phalanxCount > 1 ? totalSpread / (float)(m_params.phalanxCount - 1) : 0.0f;
                float currentAngle = startAngle + (i * angleStep);

                DirectX::XMFLOAT3 hoverPos = bPos;
                hoverPos.x += sinf(currentAngle) * m_params.phalanxHoverRadius;
                hoverPos.z += cosf(currentAngle) * m_params.phalanxHoverRadius;
                hoverPos.y += 1.0f; // Sedikit lebih tinggi dari lantai

                // Paksa pindah tanpa velocity
                m_phalanxBullets[i]->GetMovement()->SetPosition(hoverPos);
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
        // 3. FASE 2: FIRING / TEMBAK SATU PER SATU
        else if (m_phalanxState == 2) {
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
                    m_phalanxState = 0;
                    m_phalanxBullets.clear();
                }
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
}

void NaviPhaseNormal::Exit(NaviBoss* boss) { m_bulletPool.clear(); }

void NaviPhaseNormal::TriggerBijuudama(Player* targetPlayer) {
    if (!m_isLaserLocked && targetPlayer) {
        m_isLaserLocked = true;
        m_laserTimer = 0.0f;
        m_laserTargetPlayer = targetPlayer;

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
    // Hanya bisa trigger jika tidak sedang aktif
    if (m_phalanxState == 0 && targetPlayer) {
        m_phalanxState = 1; // Masuk fase Charging
        m_phalanxTimer = 0.0f;
        m_phalanxSpawned = 0;
        m_phalanxFired = 0;
        m_phalanxTarget = targetPlayer;
        m_phalanxBullets.clear(); // Kosongkan tangan bos
    }
}