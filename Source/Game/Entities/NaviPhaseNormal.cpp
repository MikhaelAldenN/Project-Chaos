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

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();

        // [FIX 1] Pastikan Main Window TIDAK AlwaysOnTop agar tidak menutupi Navi
        SDL_SetWindowAlwaysOnTop(sdlWin, false);

        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, 0, 0);
        SDL_SetWindowSize(sdlWin, screenW, screenH);
    }

    if (boss->GetMainWindow()) {
        // [FIX 2] Paksa Navi ke posisi paling depan menggunakan Win32 API
        HWND naviHwnd = boss->GetMainWindow()->GetNativeHandle();
        if (naviHwnd) {
            SetWindowPos(naviHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }

        // [FIX 3] Trigger WindowManager agar me-refresh susunan window
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

    // --- 2. Update & Recycle ---
    float distSq = m_params.despawnDist * m_params.despawnDist;
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) continue;
        bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 bossPos = boss->GetPosition();
        float dx = bossPos.x - bPos.x;
        float dz = bossPos.z - bPos.z;
        if ((dx * dx + dz * dz) > distSq) bullet->SetActive(false);
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
            if (speedSq > 900.0f) renderColor = { 0.0f, 1.0f, 1.0f, 1.0f }; // Cyan Counter

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

                // 2. Bersihkan sisa memori Homing Target (jika sebelumnya adalah peluru biasa)
                bullet->SetHomingTarget(nullptr);

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