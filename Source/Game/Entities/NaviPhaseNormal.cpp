#pragma execution_character_set("utf-8")

#include "NaviPhaseNormal.h"
#include "NaviBoss.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "System/AudioManager.h"
#include "Player.h"
#include "StateMachine.h"
#include "PlayerStates.h"
#include "WindowTrackingSystem.h"
#include "CameraController.h"
#include <SDL3/SDL.h>
#include <random>
#include "EffectManager.h"
#include "NaviPhaseWindowkill.h"
#include "WindowShatter.h"
#include <SceneBoss.h>

using namespace DirectX;

NaviPhaseNormal::NaviPhaseNormal(Player* target)
    : m_aiTarget(target)
{
}

// ============================================================
// Enter - Called once when this phase becomes active.
// ============================================================

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // --- Configure main game window ---
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();

        // Demote main window so boss window can sit above it
        mainWindow->SetPriority(50);
        SDL_SetWindowAlwaysOnTop(sdlWin, false);
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, 0, 0);

        // +1px height hack: prevents Windows from treating this as an
        // exclusive fullscreen window, keeping our Z-order control intact
        SDL_SetWindowSize(sdlWin, screenW, screenH + 1);
    }

    // Boss window stays at the top of the Z-order hierarchy
    if (boss->GetMainWindow()) {
        boss->GetMainWindow()->SetPriority(0);
        WindowManager::Instance().MarkPriorityDirty();
    }

    // Disable breathing so boss is static during the opening event
    boss->SetCoreBreathParams(1.0f, 0.0f);

    // --- Pre-allocate bullet pool ---
    m_bulletPool.clear();
    m_bulletPool.reserve(200);
    for (int i = 0; i < 200; ++i) {
        auto b = std::make_unique<Bullet>();
        b->SetActive(false);
        m_bulletPool.push_back(std::move(b));
    }

    m_zonePrimitive = std::make_unique<Primitive>(Graphics::Instance().GetDevice());

    // --- Primitives & UI ---
    m_dialogueBox = std::make_unique<UIDialogueBox>();
    m_dialogueBox->Initialize();

    // Matikan background dan atur posisi teks MELAYANG di dunia 3D (misal: di atas bos)
    m_dialogueBox->SetShowBackground(false);
    m_dialogueBox->SetWorldPosition({ -3.0f, 0.0f, 0.0f }); // X, Y, Z (Sesuaikan kordinatnya)

    m_dialogueBox->SetAutoAdvance(true, 1.5f);
    m_dialogueBox->StartDialogue({
        u8"やばっ！ウチのモデル消しちゃったんだけど～うけるwww", // "Yaba! Model gue kehapus dong~ wkwkwk"
        u8"ちょい待ち～～",                                   // "Tunggu bentar~~"
        u8"これ、どう？"                                      // "Kalo gini, gimana?"
        });

    m_isOpeningEvent = true;
    m_aiEnabled = false;

    // --- MENGATUR POSISI BOS & PLAYER ---
    if (boss) {
        boss->SetGridGrowthLimit(1.0f);
        boss->SetFaceSpriteVisible(false);
        boss->SetPosition({ 0.0f, 0.0f, 3.0f }); // Posisi awal Boss
    }

    // [NEW] Teleport Player menggunakan m_aiTarget
    if (m_aiTarget) {
        DirectX::XMFLOAT3 startPos = { 0.0f, 0.0f, -10.0f };
        m_aiTarget->SetPosition(startPos);

        // [TAMBAHKAN INI] Kunci input player
        m_aiTarget->SetInputEnabled(false);
    }
}

// ============================================================
// Exit
// ============================================================

void NaviPhaseNormal::Exit(NaviBoss* boss) {
    m_bulletPool.clear();

    if (m_bossGlitchVfxHandle != -1) {
        EffectManager::Instance().Stop(m_bossGlitchVfxHandle);
        m_bossGlitchVfxHandle = -1;
    }

    if (m_chargeEffectHandle != -1) {
        EffectManager::Instance().Stop(m_chargeEffectHandle);
        m_chargeEffectHandle = -1;
    }

    // [BARU] Matikan efek ledakan kematian jika di-respawn paksa di tengah-tengah animasi mati
    if (m_deathVfxHandle != -1) {
        EffectManager::Instance().Stop(m_deathVfxHandle);
        m_deathVfxHandle = -1;
    }
}

// ============================================================
// Update
// ============================================================

void NaviPhaseNormal::Update(float dt, NaviBoss* boss) {
    if (!boss) return;

    // =========================================================
        // [DEATH SEQUENCE] Boss Mati
        // =========================================================
    if (m_bossHP <= 0) {
        if (!m_isDying) {
            m_isDying = true;
            m_deathTimer = 0.0f;

            // 1. Matikan AI dan hentikan serangan aktif
            m_aiEnabled = false;
            CancelBijuudama();

            // 2. Mainkan VFX Kematian di posisi bos
            m_deathVfxHandle = EffectManager::Instance().Play("Data/Effect/VFX_Boss_Death.efk", boss->GetPosition(), 2.0f);

            // [MODIFIKASI BARU] Putar VFX 90 derajat menghadap ke atas (Pitch)
            if (m_deathVfxHandle != -1) {
                float rotX = DirectX::XMConvertToRadians(90.0f);
                EffectManager::Instance().SetRotation(m_deathVfxHandle, { rotX, 0.0f, 0.0f });
            }

            // 3. [PRE-LOAD] Buat pecahan window SEKARANG secara sembunyi-sembunyi agar tidak lag nanti
            DirectX::XMFLOAT3 pos = boss->GetPosition();
            WindowShatterManager::Instance().PreloadExplosion({ pos.x, pos.z }, 5);
        }

        m_deathTimer += dt;

        // Pastikan VFX selalu mengikuti posisi bos jika dia masih melayang sedikit
        if (m_deathVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_deathVfxHandle)) {
            EffectManager::Instance().SetPosition(m_deathVfxHandle, boss->GetPosition());
        }

        // 4. [MODIFIKASI BARU] Waktu tunggu diubah menjadi 5 detik
        if (m_deathTimer >= 5.0f) {
            // [MODIFIKASI BARU] Matikan paksa VFX kematian
            if (m_deathVfxHandle != -1) {
                EffectManager::Instance().Stop(m_deathVfxHandle);
                m_deathVfxHandle = -1; // Reset handle untuk keamanan memori
            }

            WindowShatterManager::Instance().WakeUpAll();
            boss->ChangePhase(std::make_unique<NaviPhaseWindowkill>(m_aiTarget));
           
        }

        return; // Hentikan sisa logika Update Normal Phase saat bos sedang mati
    }

    // --- Opening Sequence (blocks AI until dialogue ends) ---
    if (m_isOpeningEvent) {
        if (m_dialogueBox && m_dialogueBox->IsActive()) {
            m_dialogueBox->Update(dt);
            int diagIdx = m_dialogueBox->GetCurrentDialogueIndex();

            // Logika animasimu (diagIdx 0, 1, 2) biarkan utuh seperti aslinya
            if (diagIdx == 0) {
                boss->SetWindowTitle("mat_grass.png");
                boss->SetGridGrowthLimit(1.0f);
                boss->SetFaceSpriteVisible(false);
            }
            else if (diagIdx == 1) {
                if (!m_hasSpawnedWindow) {
                    boss->SpawnHeadWindow();
                    m_hasSpawnedWindow = true;
                }
                float currentLimit = boss->GetGridGrowthLimit();
                if (currentLimit < 8.0f) {
                    currentLimit = min(currentLimit + dt * 3.5f, 8.0f);
                    boss->SetGridGrowthLimit(currentLimit);
                }
                boss->SetFaceSpriteVisible(false);
            }
            else if (diagIdx == 2) {
                boss->SetGridGrowthLimit(8.0f);
                boss->SetFaceSpriteVisible(true);
            }
            return;
        }
        else {
            // [MODIFIKASI DI SINI] Dialogue finished — hand control to the AI director
            m_isOpeningEvent = false;
            m_aiEnabled = true; // Otomatis aktifkan AI

            if (m_aiTarget) {
                m_aiTarget->SetInputEnabled(true); // Bebaskan player
            }

            // Otomatis putar musik pertarungan
            AudioManager::Instance().PlayMusic("Data/Sound/BGM_Boss_Phase_01.wav", 0.05f * m_params.bgmVolumeMultiplier, true);
        
            //m_bossGlitchVfxHandle = EffectManager::Instance().Play("Data/Effect/VFX_Boss_Glitch.efk", boss->GetPosition(), 3.0f);
        }
    }
    else {
        // --------------------------------------------------------
        // [BARU] VFX Glitch Looping & Tracking (Tiap 2 Detik)
        // --------------------------------------------------------
        m_bossGlitchVfxTimer += dt;

        if (m_bossGlitchVfxTimer >= 2.0f) {
            m_bossGlitchVfxTimer -= 2.0f; // Kurangi 2 detik untuk reset presisi

            // Matikan paksa efek lama jika kebetulan masih berjalan
            if (m_bossGlitchVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_bossGlitchVfxHandle)) {
                EffectManager::Instance().Stop(m_bossGlitchVfxHandle);
            }

            // [FIX 1] Naikkan posisi Y sebesar 0.05f agar melayang DI ATAS wajah bos
            DirectX::XMFLOAT3 spawnPos = boss->GetPosition();
            spawnPos.y += 0.05f;

            // [FIX 2] Samakan scale menjadi 3.0f seperti yang kamu inginkan
            m_bossGlitchVfxHandle = EffectManager::Instance().Play("Data/Effect/VFX_Boss_Glitch.efk", spawnPos, 0.6f);

            // [FIX 3] Terapkan rotasi 90 derajat (Pitch) yang kamu temukan di TEST VFX
            if (m_bossGlitchVfxHandle != -1) {
                float rotX = DirectX::XMConvertToRadians(90.0f);
                EffectManager::Instance().SetRotation(m_bossGlitchVfxHandle, { rotX, 0.0f, 0.0f });
            }
        }

        // Terus perbarui koordinatnya setiap frame agar menempel pada Bos
        if (m_bossGlitchVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_bossGlitchVfxHandle)) {
            // PENTING: Offset Y juga harus terus dipertahankan saat objek bergerak!
            DirectX::XMFLOAT3 trackPos = boss->GetPosition();
            trackPos.y += 0.05f;
            EffectManager::Instance().SetPosition(m_bossGlitchVfxHandle, trackPos);
        }
    }

    // --------------------------------------------------------
    // [META MOMENT] LIMIT BREAKER (HP < 10%)
    // --------------------------------------------------------
    if (m_bossHP > 0 && m_bossHP <= (m_bossMaxHP * 0.10f))
    {
        if (m_aiTarget && !m_aiTarget->IsPowerUncapped())
        {
            // LEPAS BATASAN PLAYER!
            m_aiTarget->ReleasePowerCap();

            // Efek Screen Shake Ekstrem
            CameraController::Instance().AddTrauma(1.0f);

            // SFX Epik (Gunakan suara glitch/shatter yang kamu punya)
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Hit.wav", 0.5f);

            // Opsional: Gyaru Boss panik karena playernya nge-cheat/nge-hack!
            if (m_dialogueBox) {
                m_dialogueBox->SetWorldPosition({ -3.0f, 4.0f, 3.0f });
                m_dialogueBox->StartDialogue({
                    u8"えっ！？ちょっと待って！",         // "Eh!? Tunggu bentar!"
                    u8"なんで急にチート使ってんの！？", // "Kok tiba-tiba pake cheat!?"
                    u8"やばいやばいやばい！"           // "Gawat gawat gawat!"
                    });
            }
        }
    }

    // --------------------------------------------------------
    // AI Director
    // --------------------------------------------------------
    UpdateAI(dt, boss);

    // --------------------------------------------------------
    // Idle Hover (Lissajous figure-8 float)
    // --------------------------------------------------------
    m_idleHoverTimer += dt;

    // Boss floats freely when not locked into an attack state
    bool isFloating = (m_phalanxState == 0 || m_phalanxState == 5)
        && !m_isLaserLocked
        && !m_isBijuudamaRecovering;

    if (isFloating) {
        m_targetPosition.x = sinf(m_idleHoverTimer * 0.8f) * 6.0f;
        m_targetPosition.z = cosf(m_idleHoverTimer * 1.1f) * 3.0f;

        if (m_phalanxState == 0)
            m_moveLerpSpeed += (1.5f - m_moveLerpSpeed) * 2.0f * dt; // Ease toward lazy drift
    }

    // --------------------------------------------------------
    // Window Movement (Ease-In / Ease-Out)
    // --------------------------------------------------------
    // Smooth the lerp speed itself for an ease-in effect
    m_currentMoveLerpSpeed += (m_moveLerpSpeed - m_currentMoveLerpSpeed) * m_moveAcceleration * dt;

    // Apply smoothed lerp to position
    DirectX::XMFLOAT3 currentPos = boss->GetPosition();
    currentPos.x += (m_targetPosition.x - currentPos.x) * m_currentMoveLerpSpeed * dt;
    currentPos.z += (m_targetPosition.z - currentPos.z) * m_currentMoveLerpSpeed * dt;
    boss->SetPosition(currentPos);

    // --------------------------------------------------------
    // Sync Window Size to Camera Zoom
    // --------------------------------------------------------
    auto* ws = boss->GetWindowSystem();
    if (ws) {
        float newSize = 5.0f * ws->GetPixelToUnitRatio();
        boss->SetBaseWindowSize(newSize, newSize);
        boss->SetWindowSize(newSize, newSize);
    }

    // --------------------------------------------------------
    // Attack: Radial Burst (Triple)
    // --------------------------------------------------------
    if (m_isFiring) {
        if (m_burstsFired == 0) {
            FireRadialBurst(boss, 0.0f);
            m_burstsFired++;
        }
        else if (m_burstsFired < 3) {
            m_burstSequenceTimer += dt;
            if (m_burstSequenceTimer >= m_params.burstDelay) {
                m_burstSequenceTimer = 0.0f;

                // Alternate half-step offsets to create an interlocking pattern
                float step = XM_2PI / (float)m_params.count;
                float offset = (m_burstsFired % 2 == 1) ? (step * 0.5f) : 0.0f;

                FireRadialBurst(boss, offset);
                if (++m_burstsFired >= 3)
                    m_isFiring = false;
            }
        }
    }

    // --------------------------------------------------------
    // Attack: Targeted Fan Burst (Shotgun)
    // --------------------------------------------------------
    if (m_isFiringFan) {
        m_fanTimer += dt;
        if (m_fanTimer >= m_params.fanWaveDelay) {
            m_fanTimer = 0.0f;
            FireFanWave(boss);
            if (++m_fanWavesFired >= m_params.fanWaves)
                m_isFiringFan = false;
        }
    }

    // --------------------------------------------------------
    // Attack: Glintstone Phalanx
    // --------------------------------------------------------
    if (m_phalanxState > 0) {
        m_phalanxTimer += dt;
        DirectX::XMFLOAT3 bPos = boss->GetPosition();

        // Keep un-fired bullets orbiting the boss, aimed at the player
        for (int i = m_phalanxFired; i < m_phalanxSpawned; ++i) {
            if (!m_phalanxBullets[i] || !m_phalanxBullets[i]->IsActive()) continue;

            DirectX::XMFLOAT3 pPos = m_phalanxTarget->GetPosition();
            float dx = pPos.x - bPos.x;
            float dz = pPos.z - bPos.z;
            float angleToPlayer = atan2f(dx, dz);

            float totalSpread = XM_PI;
            float startOffset = -totalSpread * 0.5f;
            float step = m_params.phalanxCount > 1 ? totalSpread / (float)(m_params.phalanxCount - 1) : 0.0f;
            float bulletAngle = angleToPlayer + startOffset + (i * step);

            DirectX::XMFLOAT3 targetHoverPos = bPos;
            targetHoverPos.x += sinf(bulletAngle) * m_params.phalanxHoverRadius;
            targetHoverPos.z += cosf(bulletAngle) * m_params.phalanxHoverRadius;
            targetHoverPos.y += 1.0f;

            // Smooth bullet toward its orbit slot (ease-out)
            DirectX::XMFLOAT3 bletPos = m_phalanxBullets[i]->GetMovement()->GetPosition();
            float sSpeed = m_params.phalanxSmoothSpeed;
            bletPos.x += (targetHoverPos.x - bletPos.x) * sSpeed * dt;
            bletPos.y += (targetHoverPos.y - bletPos.y) * sSpeed * dt;
            bletPos.z += (targetHoverPos.z - bletPos.z) * sSpeed * dt;
            m_phalanxBullets[i]->GetMovement()->SetPosition(bletPos);
            m_phalanxBullets[i]->GetMovement()->SetRotationY(XMConvertToDegrees(angleToPlayer));
        }

        // State 1: Charging — wait for boss to reach its side position, then summon bullets
        if (m_phalanxState == 1) {
            float dx = m_targetPosition.x - bPos.x;
            float dz = m_targetPosition.z - bPos.z;

            if ((dx * dx + dz * dz) > 1.0f) {
                m_phalanxTimer = 0.0f; // Not at position yet; keep waiting
            }
            else {
                // Trigger rain the moment boss lands on its side
                if (m_phalanxTarget) {
                    bool isTopSide = (m_phalanxTarget->GetPosition().z > 0.0f);
                    float sweepDir = (m_targetPosition.x > 0.0f) ? -1.0f : 1.0f;
                    TriggerRainAttack(false, isTopSide, sweepDir);
                }

                // Spawn one bullet per charge tick
                if (m_phalanxTimer >= m_params.phalanxChargeDelay) {
                    m_phalanxTimer -= m_params.phalanxChargeDelay;

                    for (auto& bullet : m_bulletPool) {
                        if (!bullet->IsActive()) {
                            bullet->SetActive(true);
                            bullet->ApplyMovement(bPos, { 0, 0, 0 });
                            bullet->SetHomingTarget(nullptr);
                            bullet->SetBossTarget(nullptr);
                            bullet->SetParabolic(false);
                            bullet->SetParryReturn(false);
                            bullet->SetRadius(0.35f);
                            bullet->scale = { 2.0f, 2.0f, 2.0f };
                            bullet->SetTurnSpeed(m_params.phalanxTurnSpeed);
                            bullet->SetDamage(m_params.phalanxDamage);

                            
                            bullet->AttachVFX("Data/Effect/VFX_Boss_Phalanx_Smoke.efk", 0.3f);

                            m_phalanxBullets.push_back(bullet.get());
                            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Phalanx_Ready.wav", 0.1f * m_params.sfxVolumeMultiplier);
                            m_phalanxSpawned++;
                            break;
                        }
                    }

                    if (m_phalanxSpawned >= m_params.phalanxCount) {
                        m_phalanxState = 2;
                        m_phalanxTimer = 0.0f;
                    }
                }
            }
        }
        // State 2: Holding — all bullets orbiting, waiting before fire
        else if (m_phalanxState == 2) {
            // =========================================================
            // [BARU] Tepat 0.5 detik sebelum durasi Hold habis, Ganti Smoke jadi Flare!
            // =========================================================
            if (m_phalanxTimer >= (m_params.phalanxHoldDuration - 0.5f) && !m_phalanxFlareTriggered) {
                m_phalanxFlareTriggered = true; // Kunci agar tidak dipanggil berkali-kali tiap frame

                // Looping ke semua pedang yang sedang melayang, paksa ganti efeknya ke Flare
                for (Bullet* b : m_phalanxBullets) {
                    if (b && b->IsActive()) {
                        b->AttachVFX("Data/Effect/VFX_Boss_Phalanx_Flare.efk", 0.3f);
                    }
                }

                // Opsional: Kamu bisa tambahkan bunyi kilat/charging di sini agar lebih dramatis!
                // AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Phalanx_Charge.wav", 0.1f * m_params.sfxVolumeMultiplier);
            }

            if (m_phalanxTimer >= m_params.phalanxHoldDuration) {
                m_phalanxState = 3;
                m_phalanxTimer = 0.0f;

            }
        }
        // State 3: Firing — launch one bullet at a time toward player
        else if (m_phalanxState == 3) {
            if (m_phalanxTimer >= m_params.phalanxFireDelay) {
                m_phalanxTimer -= m_params.phalanxFireDelay;

                if (m_phalanxFired < m_phalanxSpawned) {
                    Bullet* b = m_phalanxBullets[m_phalanxFired];
                    if (b && b->IsActive()) {
                        b->SetHomingTarget(m_phalanxTarget);

                        DirectX::XMFLOAT3 myPos = b->GetMovement()->GetPosition();
                        DirectX::XMFLOAT3 pPos = m_phalanxTarget->GetPosition();
                        float dx = pPos.x - myPos.x;
                        float dz = pPos.z - myPos.z;
                        float dist = sqrtf(dx * dx + dz * dz);

                        DirectX::XMFLOAT3 dir = (dist > 0.001f) ? DirectX::XMFLOAT3{ dx / dist, 0.0f, dz / dist } : DirectX::XMFLOAT3{ 0, 0, 1 };
                        b->Fire(myPos, dir, m_params.phalanxSpeed);
                    }
                    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Phalanx_Shoot.wav", 0.1f * m_params.sfxVolumeMultiplier);
                    m_phalanxFired++;
                }

                if (m_phalanxFired >= m_phalanxSpawned) {
                    m_phalanxState = 4;
                    m_phalanxTimer = 0.0f;
                    m_phalanxBullets.clear();
                }
            }
        }        // State 4: Post-fire pause before returning to center
        else if (m_phalanxState == 4) {
            if (m_phalanxTimer >= m_params.phalanxPostFireDelay) {
                m_phalanxState = 5;
                m_targetPosition = { 0.0f, 0.0f, 0.0f };
                m_moveLerpSpeed = m_params.phalanxReturnMoveSpeed;
                m_currentMoveLerpSpeed = 0.0f;
            }
        }
        // State 5: Returning to center
        else if (m_phalanxState == 5) {
            DirectX::XMFLOAT3 pos = boss->GetPosition();
            float dx = m_targetPosition.x - pos.x;
            float dz = m_targetPosition.z - pos.z;
            if ((dx * dx + dz * dz) < 1.0f) {
                m_phalanxState = 0;
                m_phalanxBullets.clear();
            }
        }
    }

    // --------------------------------------------------------
    // Attack: Asgore Rain (Area Denial)
    // --------------------------------------------------------
    if (m_rainState > 0) {
        m_rainTimer += dt;

        // Continuous screen shake during active and dissipating rain
        if (m_rainState == 2 || m_rainState == 3)
            CameraController::Instance().AddTrauma(1.0f * dt);

        if (m_rainState == 1) { // Warning phase
            if (m_rainTimer >= m_params.rainWarningDuration) {
                m_rainState = 2;
                m_rainTimer = 0.0f;
                CameraController::Instance().AddTrauma(0.5f);

                // Play first rain sound immediately, then prime the loop timer
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Rain_01.wav", 0.07f * m_params.sfxVolumeMultiplier);
                m_rainSFXTimer = RAIN_SFX_LOOP_DURATION; // Force immediate second trigger next frame
            }
        }
        else if (m_rainState == 2) { // Active rain
            m_rainSFXTimer += dt;
            if (m_rainSFXTimer >= RAIN_SFX_LOOP_DURATION) {
                // Play 3 randomly-delayed rain sounds for an organic feel
                std::string rainSounds[] = {
                    "Data/Sound/SE_Boss_Rain_01.wav",
                    "Data/Sound/SE_Boss_Rain_02.wav",
                    "Data/Sound/SE_Boss_Rain_03.wav"
                };
                for (int i = 0; i < 3; ++i) {
                    float delay = 0.5f + ((rand() % 301) / 1000.0f);
                    AudioManager::Instance().PlaySFXDelayed(rainSounds[rand() % 3], 0.07f * m_params.sfxVolumeMultiplier, delay);
                }
                m_rainSFXTimer = 0.0f;
            }

            if (m_rainTimer >= m_params.rainActiveDuration) {
                m_rainState = 3;
                m_rainTimer = 0.0f;
            }
        }
        else if (m_rainState == 3) { // Dissipating
            m_rainSFXTimer = 0.0f;
            if (m_rainTimer >= 1.0f)
                m_rainState = 0;
        }
    }
    else {
        m_rainSFXTimer = 0.0f;
    }

    // --------------------------------------------------------
    // Attack: Bijuudama (Rhythm Laser Charge Ball)
    // --------------------------------------------------------
    if (m_isLaserLocked && m_laserTargetPlayer) {
        DirectX::XMFLOAT3 bPos = boss->GetPosition();
        float dx = m_targetPosition.x - bPos.x;
        float dz = m_targetPosition.z - bPos.z;

        // Boss hasn't reached charge position yet; hide ball and pause timer
        if ((dx * dx + dz * dz) > 1.0f) {
            m_laserTimer = 0.0f;
            if (m_bijuudamaBall) m_bijuudamaBall->SetActive(false);
        }
        else {
            // Play charge audio once when reaching position
            if (m_laserTimer == 0.0f)
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Bijuudama_Charge.wav", 0.1f * m_params.sfxVolumeMultiplier);
            if (m_chargeEffectHandle == -1) {
                DirectX::XMFLOAT3 spawnPos = m_bijuudamaBall ? m_bijuudamaBall->GetMovement()->GetPosition() : bPos;
                m_chargeEffectHandle = EffectManager::Instance().Play("Data/Effect/VFX_Boss_Bijuudama_Charge.efk", spawnPos, 1.0f);
                m_bijuudamaBall->AttachVFX("Data/Effect/VFX_Boss_Fireball.efk", m_bijuudamaBall->scale.x * 0.3f);
            }

            // Trigger dual rain pillars for the charge phase
            TriggerRainAttack(true, false, 1.0f, true);

            if (m_bijuudamaBall && !m_bijuudamaBall->IsActive())
                m_bijuudamaBall->SetActive(true);

            m_laserTimer += dt;
            DirectX::XMFLOAT3 pPos = m_laserTargetPlayer->GetPosition();

            // Grow the ball while stationary
            if (m_bijuudamaBall && m_bijuudamaBall->IsActive()) {
                DirectX::XMFLOAT3 vel = m_bijuudamaBall->GetVelocity();
                if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {
                    DirectX::XMFLOAT3 offsetPos = bPos;
                    offsetPos.z -= m_params.bijuudamaSpawnOffsetZ;
                    m_bijuudamaBall->GetMovement()->SetPosition(offsetPos);

                    float progress = min(1.0f, m_laserTimer / m_params.laserDuration);
                    float currentHitbox = m_params.bijuudamaBaseHitbox + (m_params.bijuudamaMaxHitboxGrow * progress);
                    m_bijuudamaBall->SetRadius(currentHitbox);

                    float visualScale = currentHitbox * m_params.bijuudamaVisualMultiplier;
                    m_bijuudamaBall->scale = { visualScale, visualScale, visualScale };

                    // [BARU] Sync posisi dan scale VFX agar membesar mengikuti bola!
                    if (m_chargeEffectHandle != -1 && EffectManager::Instance().IsPlaying(m_chargeEffectHandle)) {
                        EffectManager::Instance().SetPosition(m_chargeEffectHandle, offsetPos);
                        // Skalanya diperkecil sedikit dari visualScale bola agar pas di tengah
                        float vfxScale = visualScale * 0.5f;
                        EffectManager::Instance().SetScale(m_chargeEffectHandle, { vfxScale, vfxScale, vfxScale });
                    }
                }
            }

            // Grow the ball while stationary
            if (m_bijuudamaBall && m_bijuudamaBall->IsActive()) {
                DirectX::XMFLOAT3 vel = m_bijuudamaBall->GetVelocity();
                if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {
                    DirectX::XMFLOAT3 offsetPos = bPos;
                    offsetPos.z -= m_params.bijuudamaSpawnOffsetZ;
                    m_bijuudamaBall->GetMovement()->SetPosition(offsetPos);

                    float progress = min(1.0f, m_laserTimer / m_params.laserDuration);
                    float currentHitbox = m_params.bijuudamaBaseHitbox + (m_params.bijuudamaMaxHitboxGrow * progress);
                    m_bijuudamaBall->SetRadius(currentHitbox);

                    float visualScale = currentHitbox * m_params.bijuudamaVisualMultiplier;
                    m_bijuudamaBall->scale = { visualScale, visualScale, visualScale };
                }
            }

            // Visual timing ring (drawn in Update, not Render, to avoid shader state issues)
            auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
            float t = min(1.0f, m_laserTimer / m_params.laserDuration);
            float ringRadius = m_params.laserStartRadius + (m_params.laserTargetRadius - m_params.laserStartRadius) * t;

            DirectX::XMFLOAT4 ringColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // Red = not yet
            if (fabsf(m_laserTimer - m_params.laserDuration) <= m_params.laserParryWindow)
                ringColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // White = parry window open

            shapeRenderer->DrawSphere(pPos, ringRadius, ringColor);

            // Resolve: charge timer expired — fire the ball
            if (m_laserTimer >= m_params.laserDuration) {
                
                if (m_chargeEffectHandle != -1) {
                    EffectManager::Instance().Stop(m_chargeEffectHandle);
                    m_chargeEffectHandle = -1;
                }

                m_isLaserLocked = false;
                m_isBijuudamaRecovering = true;
                m_bijuudamaRecoveryTimer = 0.0f;

                if (m_bijuudamaBall && m_bijuudamaBall->IsActive()) {
                    DirectX::XMFLOAT3 vel = m_bijuudamaBall->GetVelocity();
                    if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {
                        float ddx = pPos.x - bPos.x;
                        float ddz = pPos.z - bPos.z;
                        float dist = sqrtf(ddx * ddx + ddz * ddz);
                        if (dist > 0.001f) {
                            float sSpeed = m_params.bijuudamaShootSpeed;
                            DirectX::XMFLOAT3 shootVel = { (ddx / dist) * sSpeed, 0.0f, (ddz / dist) * sSpeed };
                            DirectX::XMFLOAT3 ballPos = m_bijuudamaBall->GetMovement()->GetPosition();
                            m_bijuudamaBall->ApplyMovement(ballPos, shootVel);
                            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Bijuudama_Shoot.wav", 0.2f * m_params.sfxVolumeMultiplier);
                        }
                    }
                }
                m_bijuudamaBall = nullptr;
                m_laserTimer = 0.0f;
            }
        }
    }

    // Post-bijuudama recovery: boss hovers at top briefly, then returns to center
    if (m_isBijuudamaRecovering) {
        m_bijuudamaRecoveryTimer += dt;
        if (m_bijuudamaRecoveryTimer >= m_params.bijuudamaPostFireDelay) {
            m_isBijuudamaRecovering = false;
            m_targetPosition = { 0.0f, 0.0f, 0.0f };
            m_moveLerpSpeed = m_params.bijuudamaReturnMoveSpeed;
            m_currentMoveLerpSpeed = 0.0f;
        }
    }

    // --------------------------------------------------------
    // Bullet Pool — Update & Recycle (dynamic screen bounds)
    // --------------------------------------------------------
    float limitX = 30.0f;
    float limitZ = 20.0f;

    if (ws) {
        float p2u = ws->GetPixelToUnitRatio();
        int   sW = GetSystemMetrics(SM_CXSCREEN);
        int   sH = GetSystemMetrics(SM_CYSCREEN);
        limitX = ((sW * 0.5f) / p2u) + m_params.screenDespawnPadding;
        limitZ = ((sH * 0.5f) / p2u) + m_params.screenDespawnPadding;
    }

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) continue;
        bullet->Update(dt, nullptr);

        DirectX::XMFLOAT3 bp = bullet->GetMovement()->GetPosition();
        if (bp.x < -limitX || bp.x > limitX || bp.z < -limitZ || bp.z > limitZ)
            bullet->SetActive(false);
    }
}

// ============================================================
// Render
// ============================================================

void NaviPhaseNormal::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    if (!currentCamera) return;

    auto renderer = Graphics::Instance().GetModelRenderer();
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();

    // --- Render all active bullets ---
    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) continue;

        DirectX::XMFLOAT3 vel = bullet->GetVelocity();
        float speedSq = vel.x * vel.x + vel.z * vel.z;

        DirectX::XMFLOAT4 renderColor = m_params.color;

        if (bullet.get() == m_bijuudamaBall || bullet->GetBossTarget() != nullptr)
            renderColor = m_params.bijuudamaColor;       // Bijuudama ball or shatter fragments
        else if (speedSq > 900.0f)
            renderColor = { 1.0f, 0.0f, 0.0f, 1.0f };  // Parried bullet moving fast

        renderer->Draw(ShaderId::Phong, bullet->GetModel(), renderColor);
    }

    // --- Render Bijuudama shrinking sphere (Rhythm Laser visual) ---
    if (m_isLaserLocked && m_laserTargetPlayer) {
        DirectX::XMFLOAT3 pPos = m_laserTargetPlayer->GetPosition();
        pPos.y += 1.0f; // Chest height

        // Small cyan target dot
        shapeRenderer->DrawSphere(pPos, m_params.laserTargetRadius, { 0.0f, 1.0f, 1.0f, 1.0f });

        // Larger shrinking sphere (red → white in parry window)
        float t = min(1.0f, m_laserTimer / m_params.laserDuration);
        float shrinkRadius = m_params.laserStartRadius + (m_params.laserTargetRadius - m_params.laserStartRadius) * t;

        DirectX::XMFLOAT4 shrinkColor = { 1.0f, 0.0f, 0.0f, 1.0f };
        if (fabsf(m_laserTimer - m_params.laserDuration) <= m_params.laserParryWindow)
            shrinkColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        shapeRenderer->DrawSphere(pPos, shrinkRadius, shrinkColor);
    }

    // --- Render Asgore Rain Zone ---
    if (m_rainState > 0) {
        float actualW = GetActualRainWidth();
        float actualD = GetActualRainDepth();
        float halfW = actualW * 0.5f;
        float halfD = actualD * 0.5f;

        // Warning phase: blinking red rectangle on the ground
        if (m_rainState == 1) {
            float blink = (sinf(m_rainTimer * 20.0f) + 1.0f) * 0.5f;
            float alpha = 0.2f + (blink * 0.4f);

            float p2u = boss->GetWindowSystem()->GetPixelToUnitRatio();
            int   screenW = GetSystemMetrics(SM_CXSCREEN);
            int   screenH = GetSystemMetrics(SM_CYSCREEN);
            DirectX::XMFLOAT3 camPos = currentCamera->GetPosition();

            float width2D = actualW * p2u;
            float height2D = actualD * p2u;

            auto drawWarningRect = [&](DirectX::XMFLOAT3 centerPos) {
                float screenX = (centerPos.x - camPos.x) * p2u + (screenW * 0.5f);
                float screenY = -(centerPos.z - camPos.z) * p2u + (screenH * 0.5f);
                m_zonePrimitive->Rect(screenX, screenY, width2D, height2D, width2D * 0.5f, height2D * 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, alpha);
                };

            drawWarningRect(m_rainCenter);
            if (m_rainIsDual) drawWarningRect(m_rainCenter2);
            m_zonePrimitive->Render(context);
        }

        // Active / dissipating: procedural bullet rain (deterministic via fixed seed)
        if (m_rainState == 2 || m_rainState == 3) {
            std::mt19937 gen(1337);
            std::uniform_real_distribution<float> distSpeed(m_params.rainMinSpeed, m_params.rainMaxSpeed);
            std::uniform_real_distribution<float> distSpawn(0.0f, m_params.rainActiveDuration);

            float currentGlobalTime = (m_rainState == 2)
                ? m_rainTimer
                : (m_params.rainActiveDuration + m_rainTimer);

            int dropCount = m_rainIsDual ? 800 : 400;

            for (int i = 0; i < dropCount; ++i) {
                float speed = distSpeed(gen);
                float spawnTime = distSpawn(gen);
                float localTime = currentGlobalTime - spawnTime;
                if (localTime < 0.0f) continue;

                if (m_rainIsDual) {
                    // Alternate drops between left and right pillars
                    DirectX::XMFLOAT3 activeCenter = (i % 2 == 0) ? m_rainCenter : m_rainCenter2;
                    std::uniform_real_distribution<float> distX(activeCenter.x - halfW, activeCenter.x + halfW);
                    float rx = distX(gen);
                    float topEdge = activeCenter.z + halfD + 5.0f;
                    float bottomEdge = activeCenter.z - halfD;
                    float z = topEdge - (localTime * speed);
                    if (z >= bottomEdge)
                        shapeRenderer->DrawSphere({ rx, 1.0f, z }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                }
                else if (m_rainIsVertical) {
                    // Vertical zone: drops fall top-to-bottom along Z
                    std::uniform_real_distribution<float> distX(m_rainCenter.x - halfW, m_rainCenter.x + halfW);
                    float rx = distX(gen);
                    float topEdge = m_rainCenter.z + halfD + 5.0f;
                    float bottomEdge = m_rainCenter.z - halfD;
                    float z = topEdge - (localTime * speed);
                    if (z >= bottomEdge)
                        shapeRenderer->DrawSphere({ rx, 1.0f, z }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                }
                else {
                    // Horizontal zone: drops sweep left-to-right or right-to-left
                    std::uniform_real_distribution<float> distZ(m_rainCenter.z - halfD, m_rainCenter.z + halfD);
                    float rz = distZ(gen);
                    float dir = m_rainSweepDir;
                    float startX = (dir > 0) ? (m_rainCenter.x - halfW - 5.0f) : (m_rainCenter.x + halfW + 5.0f);
                    float endX = (dir > 0) ? (m_rainCenter.x + halfW) : (m_rainCenter.x - halfW);
                    float x = startX + (localTime * speed * dir);
                    bool  inside = (dir > 0) ? (x <= endX) : (x >= endX);
                    if (inside)
                        shapeRenderer->DrawSphere({ x, 1.0f, rz }, 0.4f, { 1.0f, 0.4f, 0.0f, 1.0f });
                }
            }
        }
    }

    // --- UI (rendered last, on top of everything) ---
    if (m_dialogueBox && m_dialogueBox->IsActive()) {
        // [FIX] Gunakan Render3D dan passing Camera agar bisa dikalkulasi perspektifnya
        m_dialogueBox->Render3D(context, currentCamera);
    }
}

// ============================================================
// Attack Triggers
// ============================================================

void NaviPhaseNormal::TriggerTripleBurst() {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiring = true;
        m_burstsFired = 0;
        m_burstSequenceTimer = 0.0f;
    }
}

void NaviPhaseNormal::TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos) {
    if (!m_isFiring && !m_isFiringFan) {
        m_isFiringFan = true;
        m_fanWavesFired = 0;
        m_fanTimer = m_params.fanWaveDelay; // Full timer so first wave fires immediately

        // Lock the aim angle to the player's position at trigger time
        DirectX::XMFLOAT3 bossPos = boss->GetPosition();
        m_lockedBaseAngle = atan2f(playerPos.x - bossPos.x, playerPos.z - bossPos.z);
    }
}

void NaviPhaseNormal::TriggerBijuudama(Player* targetPlayer) {
    if (!m_isLaserLocked && targetPlayer) {
        m_isLaserLocked = true;
        m_laserTimer = 0.0f;
        m_laserTargetPlayer = targetPlayer;

        // Boss slides to top center to charge
        m_targetPosition = { 0.0f, 0.0f, 10.0f };
        m_moveLerpSpeed = m_params.bijuudamaAttackMoveSpeed;
        m_currentMoveLerpSpeed = 0.0f;

        // Claim a bullet from the pool for the ball
        for (auto& bullet : m_bulletPool) {
            if (!bullet->IsActive()) {
                bullet->SetActive(true);

                // Full reset to clear any leftover state from previous use
                bullet->ApplyMovement({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
                bullet->SetHomingTarget(nullptr);
                bullet->SetBossTarget(nullptr);
                bullet->SetParabolic(false);
                bullet->SetParryReturn(false);
                bullet->SetTurnSpeed(8.0f);

                float baseHitbox = m_params.bijuudamaBaseHitbox;
                float baseVisual = baseHitbox * m_params.bijuudamaVisualMultiplier;
                bullet->SetRadius(baseHitbox);
                bullet->scale = { baseVisual, baseVisual, baseVisual };
                bullet->SetDamage(m_params.bijuudamaBallDamage);

                //bullet->AttachVFX("Data/Effect/VFX_Boss_Fireball.efk", 1.0f);

                m_bijuudamaBall = bullet.get();
                break;
            }
        }
    }
}

void NaviPhaseNormal::TriggerPhalanx(Player* targetPlayer) {
    if (m_phalanxState == 0 && targetPlayer) {
        m_phalanxState = 1;
        m_phalanxTimer = 0.0f;
        m_phalanxSpawned = 0;
        m_phalanxFired = 0;
        m_phalanxFlareTriggered = false;
        m_phalanxTarget = targetPlayer;
        m_phalanxBullets.clear();

        // Randomly pick left or right side
        std::mt19937 gen(std::random_device{}());
        m_targetPosition.x = (std::uniform_int_distribution<>(0, 1)(gen) == 0) ? -15.0f : 15.0f;
        m_targetPosition.z = 0.0f;
        m_moveLerpSpeed = m_params.phalanxAttackMoveSpeed;
        m_currentMoveLerpSpeed = 0.0f;
    }
}

void NaviPhaseNormal::TriggerRainAttack(bool isSideMode, bool isPositiveSide, float sweepDir, bool isDual) {
    if (m_rainState != 0) return;

    m_rainState = 1;
    m_rainTimer = 0.0f;
    m_rainIsVertical = isSideMode;
    m_rainSweepDir = sweepDir;
    m_rainIsDual = isDual;

    if (isDual) {
        // Two vertical pillars, left and right
        m_rainCenter = { -16.0f, 0.0f, 0.0f };
        m_rainCenter2 = { 16.0f, 0.0f, 0.0f };
    }
    else if (!isSideMode) {
        // Horizontal zone at top or bottom
        m_rainCenter = { 0.0f, 0.0f, isPositiveSide ? 7.5f : -7.5f };
    }
    else {
        // Vertical zone at left or right
        m_rainCenter = { isPositiveSide ? 12.5f : -12.5f, 0.0f, 0.0f };
    }
}

void NaviPhaseNormal::TakeDamage(int damage, DirectX::XMFLOAT3 hitPos) {
    if (m_bossHP <= 0) return;
    m_bossHP = max(0, m_bossHP - damage);
    m_hitFlashTimer = 0.05f;

    // Micro-shake on hit feedback
    CameraController::Instance().AddTrauma(0.3f);
    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Hit.wav", 0.1f * m_params.sfxVolumeMultiplier);

    // [BARU] Putar VFX tepat di titik tabrakan peluru dengan bos
    EffectManager::Instance().Play("Data/Effect/VFX_Boss_Hit.efk", hitPos, 0.3f);
}

void NaviPhaseNormal::ShatterBijuudama(DirectX::XMFLOAT3 parryPos, NaviBoss* boss) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<>  distCount(m_params.shatterMinFragments, m_params.shatterMaxFragments);
    std::uniform_real_distribution<float> distSize(m_params.shatterMinRadius, m_params.shatterMaxRadius);
    std::uniform_real_distribution<float> distDur(m_params.shatterMinDuration, m_params.shatterMaxDuration);
    std::uniform_real_distribution<float> distAngle(-XM_PIDIV2, XM_PIDIV2);

    int fragments = distCount(gen);
    int spawned = 0;

    DirectX::XMFLOAT3 bossPos = boss->GetPosition();
    float baseAngle = atan2f(bossPos.x - parryPos.x, bossPos.z - parryPos.z);

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            bullet->SetActive(true);
            bullet->ApplyMovement(parryPos, { 0, 0, 0 });
            bullet->SetBossTarget(boss);
            bullet->SetParabolic(true);
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);

            float r = distSize(gen);
            bullet->SetRadius(r);
            bullet->scale = { r * 3.0f, r * 3.0f, r * 3.0f };

            // Bezier control point: fan out from parry position toward boss
            float spreadAngle = baseAngle + distAngle(gen);
            DirectX::XMFLOAT3 ctrlPoint = {
                parryPos.x + sinf(spreadAngle) * m_params.shatterCurveOffset,
                parryPos.y,
                parryPos.z + cosf(spreadAngle) * m_params.shatterCurveOffset
            };

            bullet->SetParabolaParams(parryPos, ctrlPoint, distDur(gen));
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Player_Parry_Boss.wav", 0.015f * m_params.sfxVolumeMultiplier);

            if (++spawned >= fragments) break;
        }
    }
}

// ============================================================
// AI Director
// ============================================================

void NaviPhaseNormal::UpdateAI(float dt, NaviBoss* boss) {
    if (!m_aiEnabled || !m_aiTarget) return;

    std::mt19937 gen(std::random_device{}());

    // --- Track A: Side Rain (runs independently of main attacks) ---
    m_cdRain -= dt;
    if (m_cdRain <= 0.0f && !m_isLaserLocked && m_phalanxState == 0) {
        bool isRightSide = (m_aiTarget->GetPosition().x > 0.0f);
        TriggerRainAttack(true, isRightSide);
        m_cdRain = std::uniform_real_distribution<float>(8.0f, 15.0f)(gen);
    }

    // --- Track B: Primary Attacks (priority queue, one at a time) ---
    bool isBusy = m_isFiring || m_isFiringFan
        || (m_phalanxState > 0 && m_phalanxState < 5)
        || m_isLaserLocked || m_isBijuudamaRecovering;

    if (!isBusy) {
        if (m_aiGlobalCooldown > 0.0f)
            m_aiGlobalCooldown -= dt;
        else {
            m_cdRadial -= dt;
            m_cdFan -= dt;
            m_cdPhalanx -= dt;
            m_cdBijuudama -= dt;
        }
    }

    if (!isBusy && m_aiGlobalCooldown <= 0.0f) {
        if (m_cdBijuudama <= 0.0f && m_rainState == 0) {
            // Priority 1: Bijuudama
            TriggerBijuudama(m_aiTarget);
            m_cdBijuudama = std::uniform_real_distribution<float>(15.0f, 25.0f)(gen);
            m_aiGlobalCooldown = 0.5f;
        }
        else if (m_cdPhalanx <= 0.0f && m_rainState == 0) {
            // Priority 2: Glintstone Phalanx
            TriggerPhalanx(m_aiTarget);
            m_cdPhalanx = std::uniform_real_distribution<float>(6.0f, 10.0f)(gen);
            m_aiGlobalCooldown = 0.2f;
        }
        else if (m_cdFan <= 0.0f) {
            // Priority 3: Fan Wave (Shotgun)
            TriggerFanAttack(boss, m_aiTarget->GetPosition());
            m_cdFan = std::uniform_real_distribution<float>(4.0f, 8.0f)(gen);
            m_aiGlobalCooldown = 0.1f;
        }
        else if (m_cdRadial <= 0.0f) {
            // Priority 4: Radial Burst (always fires as a triple)
            TriggerTripleBurst();
            m_cdRadial = std::uniform_real_distribution<float>(2.0f, 5.0f)(gen);
            m_aiGlobalCooldown = 0.1f;
        }
    }
}

// ============================================================
// Internal Fire Helpers
// ============================================================

void NaviPhaseNormal::FireRadialBurst(NaviBoss* boss, float angleOffset) {
    int   firedCount = 0;
    float angleStep = XM_2PI / (float)m_params.count;

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            // Reset to standard bullet state before firing
            bullet->SetRadius(0.25f);
            bullet->scale = { 1.0f, 1.0f, 1.0f };
            bullet->SetHomingTarget(nullptr);
            bullet->SetBossTarget(nullptr);
            bullet->SetParabolic(false);
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);

            // [PENTING] Set Damage SEBELUM memanggil Fire!
            bullet->SetDamage(m_params.radialDamage);

            float angle = (firedCount * angleStep) + angleOffset;
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.radialSpeed);

            if (++firedCount >= m_params.count) break;
        }
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolumeMultiplier);
}

void NaviPhaseNormal::FireFanWave(NaviBoss* boss) {
    int   lines = m_params.fanLines;
    float spread = m_params.fanSpreadAngle;
    float startAngle = m_lockedBaseAngle - ((lines - 1) * spread * 0.5f);
    int   firedCount = 0;

    for (auto& bullet : m_bulletPool) {
        if (!bullet->IsActive()) {
            // Reset to standard bullet state before firing
            bullet->SetRadius(0.25f);
            bullet->scale = { 1.0f, 1.0f, 1.0f };
            bullet->SetHomingTarget(nullptr);
            bullet->SetBossTarget(nullptr);
            bullet->SetParabolic(false);
            bullet->SetTurnSpeed(8.0f);

            float angle = startAngle + (firedCount * spread);
            bullet->Fire(boss->GetPosition(), { sinf(angle), 0.0f, cosf(angle) }, m_params.fanSpeed);
            bullet->SetDamage(m_params.fanDamage); // [BARU] Pasang damage radial
            if (++firedCount >= lines) break;
        }
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolumeMultiplier);
}