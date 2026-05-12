#include "NaviPhaseNormal.h"
#include "NaviBoss.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include <SDL3/SDL.h>

#include "System/Input.h"
#include "Player.h"
#include "StateMachine.h"
#include "PlayerStates.h"

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    // (Logika Borderless Fullscreen sama)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowResizable(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, 0, 0);
        SDL_SetWindowSize(sdlWin, screenW, screenH);
    }

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

    // =========================================================
        // LOGIKA RHYTHM LASER PARRY (GHOST BULLET SYSTEM)
        // =========================================================
    if (m_isLaserLocked && m_laserTargetPlayer) {
        m_laserTimer += dt;
        bool ghostExists = false;

        // 1. Cari Ghost Bullet dan tempelkan terus ke dada player
        for (auto& bullet : m_bulletPool) {
            if (bullet->IsActive()) {
                DirectX::XMFLOAT3 vel = bullet->GetVelocity();
                float speedSq = (vel.x * vel.x) + (vel.z * vel.z);

                if (speedSq < 0.01f) { // Jika kecepatannya 0 (Hantu)
                    ghostExists = true;
                    DirectX::XMFLOAT3 pPos = m_laserTargetPlayer->GetPosition();
                    pPos.y += 1.0f;
                    bullet->GetMovement()->SetPosition(pPos); // Sinkronisasi posisi
                    break;
                }
            }
        }

        // 2. Evaluasi Status Laser
        if (!ghostExists) {
            // Jika hantu hilang (Artinya player berhasil mem-Parry-nya hingga kecepatannya tidak 0 lagi!)
            m_isLaserLocked = false;
        }
        else if (m_laserTimer > m_params.laserDuration) {
            // Waktu habis dan hantu masih ada (Gagal Parry!)
            m_isLaserLocked = false;
            m_laserTargetPlayer->TakeDamage(m_params.laserDamage);

            // Bersihkan hantunya agar layar bersih
            for (auto& bullet : m_bulletPool) {
                if (bullet->IsActive()) {
                    DirectX::XMFLOAT3 vel = bullet->GetVelocity();
                    if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) bullet->SetActive(false);
                }
            }
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

            // [FIX] JANGAN RENDER GHOST BULLET! (Kecepatan 0)
            if (speedSq < 0.01f) continue;

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

void NaviPhaseNormal::TriggerLockingLaser(Player* targetPlayer) {
    if (!m_isLaserLocked && targetPlayer) {
        m_isLaserLocked = true;
        m_laserTimer = 0.0f;
        m_laserTargetPlayer = targetPlayer;

        // =========================================================
        // [MAGIC] SPAWN THE GHOST BULLET
        // =========================================================
        for (auto& bullet : m_bulletPool) {
            if (!bullet->IsActive()) {
                DirectX::XMFLOAT3 pPos = targetPlayer->GetPosition();
                pPos.y += 1.0f;
                // Tembakkan peluru dengan kecepatan 0.0f (Hantu diam)
                bullet->Fire(pPos, { 0.0f, 0.0f, 1.0f }, 0.0f);
                break;
            }
        }
    }
}