#include "SceneGame.h" 

#include "CameraController.h"
#include "CinematicDirector.h" 
#include "Framework.h"
#include "GUISceneGameBreaker.h" 
#include "PostProcessManager.h"
#include "Primitive.h"
#include "ResourceManager.h"
#include "System/AudioManager.h"
#include "System/CollisionManager.h"
#include "System/Graphics.h"
#include "InputHelper.h"
#include <algorithm>

// Game Objects
#include "EffectManager.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "NaviAlly.h"
#include "Player.h"
#include "PlayerStates.h"
#include "Stage.h"

using namespace DirectX;

namespace
{
    size_t NextUtf8Offset(const std::string& text, size_t offset)
    {
        if (offset >= text.size()) return text.size();

        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        size_t length = 1;

        if ((lead & 0x80) == 0x00) length = 1;
        else if ((lead & 0xE0) == 0xC0) length = 2;
        else if ((lead & 0xF0) == 0xE0) length = 3;
        else if ((lead & 0xF8) == 0xF0) length = 4;

        return (std::min)(offset + length, text.size());
    }

    int CountUtf8Characters(const std::string& text)
    {
        int count = 0;
        for (size_t offset = 0; offset < text.size(); offset = NextUtf8Offset(text, offset))
        {
            ++count;
        }
        return count;
    }

    std::string Utf8Prefix(const std::string& text, int characterCount)
    {
        if (characterCount <= 0) return {};

        size_t offset = 0;
        int count = 0;
        while (offset < text.size() && count < characterCount)
        {
            offset = NextUtf8Offset(text, offset);
            ++count;
        }
        return text.substr(0, offset);
    }
}

SceneGame::SceneGame()
{
    float screenW{ Config::DEFAULT_SCREEN_W };
    float screenH{ Config::DEFAULT_SCREEN_H };

    if (auto window{ Framework::Instance()->GetMainWindow() }) {
        SDL_Window* sdlWin = window->GetSDLWindow();

        // Disable window borders and the ability to resize
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowResizable(sdlWin, false);

        // Grab monitor size and force the window to match it perfectly
        int fullW = GetSystemMetrics(SM_CXSCREEN);
        int fullH = GetSystemMetrics(SM_CYSCREEN);
        SDL_SetWindowSize(sdlWin, fullW, fullH);
        SDL_SetWindowPosition(sdlWin, 0, 0);

        // Update local configuration variables
        screenW = static_cast<float>(fullW);
        screenH = static_cast<float>(fullH);
    }

    auto& camCtrl{ CameraController::Instance() };
    camCtrl.ClearCamera();
    camCtrl.StopSequence();
    camCtrl.SetTargetOffset({ 0.0f, 0.0f, 0.0f });
    camCtrl.SetFixedYawOffset(0.0f);
    camCtrl.SetFixedRollOffset(0.0f);
    camCtrl.SetSplineTension(1.0f);

    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(Config::CAM_FOV), screenW / screenH, Config::CAM_NEAR, Config::CAM_FAR);

    XMFLOAT3 startPos{ m_cameraPosition };
    startPos.x = 0.0f;
    startPos.z = -14.0f;
    startPos.y = Config::CAM_START_HEIGHT;

    m_mainCamera->SetPosition(startPos);
    m_mainCamera->LookAt(m_cameraTarget);
    camCtrl.SetActiveCamera(m_mainCamera);
    camCtrl.SetControlMode(CameraControlMode::FixedFollow);
    camCtrl.SetFixedSetting(startPos);
    camCtrl.SetTarget(m_cameraTarget);

    m_stage = std::make_unique<Stage>(Graphics::Instance().GetDevice());

    m_foundation.reset(PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback));
    assert(m_foundation != nullptr && "CRITICAL ERROR: PxCreateFoundation failed!");

    m_physics.reset(PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale(), true, nullptr));
    assert(m_physics != nullptr && "CRITICAL ERROR: PxCreatePhysics failed!");

    physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, Config::GRAVITY, 0.0f);

    m_dispatcher.reset(physx::PxDefaultCpuDispatcherCreate(2));
    sceneDesc.cpuDispatcher = m_dispatcher.get();
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

    m_scene.reset(m_physics->createScene(sceneDesc));
    assert(m_scene != nullptr && "CRITICAL ERROR: createScene failed!");

    m_controllerManager.reset(PxCreateControllerManager(*m_scene));
    assert(m_controllerManager != nullptr && "CRITICAL ERROR: PxCreateControllerManager failed!");

    m_defaultMaterial.reset(m_physics->createMaterial(0.5f, 0.5f, 0.1f));
    assert(m_defaultMaterial != nullptr && "CRITICAL ERROR: createMaterial failed!");

    m_groundPlane.reset(physx::PxCreatePlane(*m_physics, physx::PxPlane(0, 1, 0, 0), *m_defaultMaterial));
    m_scene->addActor(*m_groundPlane);

    m_player = std::make_unique<Player>();

    m_player->SetPosition(m_playerSpawnPos);
    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get());
    m_stage->InitPhysics(m_physics.get(), m_scene.get(), m_defaultMaterial.get());

    PlayerConfig gameConfig{};
    gameConfig.moveSpeed = 8.0f;
    gameConfig.dashSpeed = 28.0f;

    m_player->ApplyConfig(gameConfig);
    m_player->GetMovement()->SetRotationY(DirectX::XM_PI);

    m_enemyManager = std::make_unique<EnemyManager>();
    m_enemyManager->Initialize(Graphics::Instance().GetDevice());

    m_navi = std::make_unique<NaviAlly>(Graphics::Instance().GetDevice(), m_player.get(), m_enemyManager.get());

    m_itemManager = std::make_unique<ItemManager>();
    m_itemManager->Initialize(Graphics::Instance().GetDevice());

    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), m_enemyManager.get(), m_itemManager.get());
    m_collisionManager->SetNavi(m_navi.get());

    m_player->SetCollisionManager(m_collisionManager.get());
    m_collisionManager->SetOnEnableLineReachCallback([this](int lineIndex) {
        if (lineIndex == 0 && !m_bossCinematicTriggered)
        {
            if (AreTrackingEnemiesDead())
            {
                StartBossCinematic();
            }
        }
        });

    m_director = std::make_unique<CinematicDirector>();

    m_postProcess = std::make_unique<PostProcessManager>();
    m_postProcess->Initialize(static_cast<int>(screenW), static_cast<int>(screenH));

    m_dialogueBox = std::make_unique<UIDialogueBox>();

    m_dialogueBox->Initialize();

    m_fadeSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Game/Black.png");
    m_whiteSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Game/White.png");
    EffectManager::Instance().PreloadEffect("Data/Effect/Hit.efk");
    EffectManager::Instance().PreloadEffect("Data/Effect/FakeBossPoison.efk");
}

SceneGame::~SceneGame()
{
    AudioManager::Instance().StopMusic();
    //CameraController::Instance().ClearCamera();

    m_player.reset();
    m_stage.reset();
    m_enemyManager.reset();
    m_itemManager.reset();
}

void SceneGame::Update(const float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > Config::TIME_LOOP_MAX) m_globalTime -= Config::TIME_LOOP_MAX;

    if (m_navi && !m_navi->IsAlive() && !m_isNaviDefeatSequenceActive)
    {
        StartNaviDefeatSequence();
    }

    if (m_player && m_player->GetHP() <= 0 && !m_isDying && !m_isNaviDefeatSequenceActive && m_respawnTimer <= 0.0f)
    {
        StartPlayerDeathSequence();
    }

    if (m_isNaviDefeatSequenceActive)
    {
        m_naviDefeatTimer += elapsedTime;

        const float linearT{ std::clamp(m_naviDefeatTimer / NAVI_DEFEAT_FADE_DURATION, 0.0f, 1.0f) };
        const float t{ linearT * linearT * (3.0f - 2.0f * linearT) };

        m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
        m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;
        m_fadeAlpha = t;

        if (linearT >= 1.0f)
        {
            m_uberParams.smoothness = FX_BLACK_SMOOTHNESS;
            m_uberParams.intensity = FX_BLACK_INTENSITY;
            m_fadeAlpha = 1.0f;
            m_isNaviDefeatReadyForNextScene = true;
            m_player.reset();
            m_navi.reset();
            m_enemyManager.reset();
            m_itemManager.reset();
            m_stage.reset();
            m_collisionManager.reset();

            // Destroy PhysX core components in reverse order of creation
            m_groundPlane.reset();
            m_defaultMaterial.reset();
            m_controllerManager.reset();
            m_scene.reset();
            m_dispatcher.reset();
            m_physics.reset();
            m_foundation.reset(); // <-- This frees PxFoundation for SceneBoss!

            // Clear the camera so SceneBoss can claim it without it getting overridden
            CameraController::Instance().ClearCamera();
            Framework::Instance()->ChangeScene(std::make_unique<SceneBoss>());

            return;
        }
    }
    else if (m_bootTimer > 0.0f)
    {
        m_bootTimer -= elapsedTime;
        m_fadeAlpha = 1.0f;
        m_uberParams.smoothness = FX_BLACK_SMOOTHNESS;
        m_uberParams.intensity = FX_BLACK_INTENSITY;

        if (m_player)
        {
            m_player->SetInputEnabled(false);
            CameraController::Instance().SetTarget(m_player->GetPosition());
            CameraController::Instance().Update(0.0f);
        }

        if (m_bootTimer <= 0.0f && m_player)
        {
            m_player->SetInputEnabled(true);
        }
    }

    else if (m_isDying)
    {
        m_deathTimer += elapsedTime;

        if (m_deathTimer < DEATH_DELAY_DURATION)
        {
            m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
            m_uberParams.intensity = FX_BASE_INTENSITY;
            m_fadeAlpha = 0.0f;
        }
        else
        {
            const float fadeTime{ m_deathTimer - DEATH_DELAY_DURATION };
            const float t{ std::clamp(fadeTime / DEATH_FADE_DURATION, 0.0f, 1.0f) };

            // LERP towards pitch black
            m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
            m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;

            m_fadeAlpha = t;

            if (t >= 1.0f)
            {
                ResetLevel(); // Perform the zero-cost reset
                m_isDying = false;
                m_respawnTimer = RESPAWN_FADE_DURATION;

                // Force screen to stay black for the first frame of respawn
                m_uberParams.smoothness = FX_BLACK_SMOOTHNESS;
                m_uberParams.intensity = FX_BLACK_INTENSITY;
                m_fadeAlpha = 1.0f;
            }
        }
    }
    else if (m_respawnTimer > 0.0f)
    {
        m_respawnTimer -= elapsedTime;

        if (m_player) m_player->SetInputEnabled(false);

        // Quadratic Ease-Out for a smoother fade-in curve
        const float linearT{ std::clamp(m_respawnTimer / RESPAWN_FADE_DURATION, 0.0f, 1.0f) };
        const float t{ linearT * linearT };

        m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
        m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;
        m_fadeAlpha = t;

        if (m_respawnTimer <= 0.0f && m_player)
        {
            m_player->SetInputEnabled(true);
        }
    }
    else
    {
        // Normal Gameplay Lighting
        m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
        m_fadeAlpha = 0.0f;

        if (!m_hasBGMStarted)
        {
            AudioManager::Instance().PlayMusic("Data/Sound/BGM_Game.wav", 0.1f, true);

            m_hasBGMStarted = true;
        }
    }

    if (!m_hasIntroDialogueTestStarted &&
        m_bootTimer <= 0.0f &&
        m_respawnTimer <= 0.0f &&
        !m_isDying &&
        !m_isNaviDefeatSequenceActive)
    {
        StartIntroDialogueTest();
    }

    if (m_dialogueBox)
    {
        m_dialogueBox->Update(elapsedTime);
    }

    if (m_scene) {
        m_scene->simulate(elapsedTime);
        m_scene->fetchResults(true);
    }

    Camera* activeCam{ CameraController::Instance().GetActiveCamera().get() };

    if (m_player) {
        if (m_mainCamera)
        {
            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);

            float screenW = Config::DEFAULT_SCREEN_W;
            float screenH = Config::DEFAULT_SCREEN_H;
            if (auto window = Framework::Instance()->GetMainWindow()) {
                screenW = static_cast<float>(window->GetWidth());
                screenH = static_cast<float>(window->GetHeight());
            }

            DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&m_mainCamera->GetView());
            DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&m_mainCamera->GetProjection());
            DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

            DirectX::XMVECTOR nearPoint = DirectX::XMVectorSet(mouseX, mouseY, 0.0f, 0.0f);
            DirectX::XMVECTOR farPoint = DirectX::XMVectorSet(mouseX, mouseY, 1.0f, 0.0f);

            nearPoint = DirectX::XMVector3Unproject(nearPoint, 0, 0, screenW, screenH, 0.0f, 1.0f, proj, view, world);
            farPoint = DirectX::XMVector3Unproject(farPoint, 0, 0, screenW, screenH, 0.0f, 1.0f, proj, view, world);

            DirectX::XMVECTOR rayDir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
            DirectX::XMFLOAT3 origin, dir;
            DirectX::XMStoreFloat3(&origin, nearPoint);
            DirectX::XMStoreFloat3(&dir, rayDir);

            if (abs(dir.y) > 0.001f) {
                float t = -origin.y / dir.y;
                DirectX::XMFLOAT3 trueMouseWorldPos = { origin.x + dir.x * t, 0.0f, origin.z + dir.z * t };
                m_player->RotateModelToPoint(trueMouseWorldPos);
            }
        }

        m_player->Update(elapsedTime, activeCam);
        if (m_navi) m_navi->Update(elapsedTime, activeCam);
    }

    if (m_enemyManager) {
        XMFLOAT3 targetPos{ 0.0f, 0.0f, 0.0f };
        if (m_player) {
            targetPos = m_player->GetPosition();
        }
        bool canAttack = (m_player && m_player->GetHP() > 0);
        m_enemyManager->Update(elapsedTime, activeCam, targetPos, canAttack);
    }

    if (m_itemManager) m_itemManager->Update(elapsedTime, activeCam);
    if (m_collisionManager) m_collisionManager->Update(elapsedTime);

    // Furi style cinematic combat camera 
    static float targetZoom{ 0.0f };
    static int   frameCounter{ 0 };
    static const Enemy* cachedClosestEnemy{ nullptr };

    if (m_isBossCinematicActive)
    {
        // 1. Hitung pergerakan kamera (berhenti di angka 1.0)
        float t = std::clamp(m_bossCinematicTimer / BOSS_CINEMATIC_DURATION, 0.0f, 1.0f);
        float smoothT = t * t * (3.0f - 2.0f * t);

        DirectX::XMFLOAT3 currentTarget = {
            m_cinematicStartTarget.x + (m_cinematicEndTarget.x - m_cinematicStartTarget.x) * smoothT,
            m_cinematicStartTarget.y + (m_cinematicEndTarget.y - m_cinematicStartTarget.y) * smoothT,
            m_cinematicStartTarget.z + (m_cinematicEndTarget.z - m_cinematicStartTarget.z) * smoothT
        };

        CameraController::Instance().SetDynamicZoomOffset(0.0f);
        CameraController::Instance().SetTarget(currentTarget);

        // 2. State Machine Cinematic
        if (m_bossCinematicTimer < BOSS_CINEMATIC_DURATION)
        {
            // FASE 1: Kamera masih jalan menuju Boss. Timer jalan terus.
            m_bossCinematicTimer += elapsedTime;
        }
        else if (!m_bossDialogueStarted)
        {
            // FASE 2: Kamera sampai. Munculin dialog dan STOP Timer!
            m_bossDialogueStarted = true;
            std::vector<std::string> dialogPages = {
                u8"えっ…？ 何あのキノコ…。\n他のやつらより、ずっと大きい……？",
                u8"ちょっと待って、様子がおかしいわ。\nなんか…膨らんでない！？",
                u8"きゃあああああっ！？\n毒ガス！？ ごほっ、げほっ…！"
            };
            // Terjemahan kasarnya:
            // 1. Eh...? Jamur apa itu... Jauh lebih besar dari yang lain?
            // 2. Tunggu, ada yang aneh. Kok dia... membesar?!
            // 3. Kyaaaa?! Gas racun?! Uhuk, uhuk!

            m_dialogueBox->StartDialogue(dialogPages);
        }
        else if (m_bossDialogueStarted)
        {
            // FASE 3: Dialog sedang berjalan, nungguin player mencet Enter
            int currentLine = m_dialogueBox->GetCurrentDialogueIndex();

            // Pas nyampe di baris ke-3 (index 2) dan efek belum keluar, TRIGGER RACUNNYA!
            if (currentLine == 2 && !m_bossEffectTriggered)
            {
                m_bossEffectTriggered = true;

                static const std::string POISON_SFX{ "Data/Sound/SE_FakeBoss_Poison.wav" };

                AudioManager::Instance().PlayAmbientSFX(POISON_SFX, 1.0f, 0.5f);

                if (Enemy* fakeBoss = GetFakeBoss())
                {
                    DirectX::XMFLOAT3 spawnPos = fakeBoss->GetPosition();
                    spawnPos.x += m_fakeBossEffectOffset.x;
                    spawnPos.y += m_fakeBossEffectOffset.y;
                    spawnPos.z += m_fakeBossEffectOffset.z;

                    Effekseer::Handle effHandle = EffectManager::Instance().Play(
                        "Data/Effect/FakeBossPoison.efk", spawnPos, m_fakeBossEffectScale
                    );

                    if (effHandle >= 0) {
                        DirectX::XMFLOAT3 rotRad{
                            DirectX::XMConvertToRadians(m_fakeBossEffectRotation.x),
                            DirectX::XMConvertToRadians(m_fakeBossEffectRotation.y),
                            DirectX::XMConvertToRadians(m_fakeBossEffectRotation.z)
                        };
                        EffectManager::Instance().SetRotation(effHandle, rotRad);
                    }
                }
            }

            // FASE 4: Player mencet enter nutup jeritan Navi. Kotak dialog otomatis hilang, mulai Fade Putih!
            if (m_bossEffectTriggered && !m_dialogueBox->IsActive())
            {
                AudioManager::Instance().FadeOutAmbientSFX(1.5f);

                // Jalanin timer lagi buat ngitung efek whiteout
                m_bossCinematicTimer += elapsedTime;

                float timeInFade = m_bossCinematicTimer - BOSS_CINEMATIC_DURATION;

                if (timeInFade < WHITEOUT_FADE_DURATION)
                {
                    float linearT = std::clamp(timeInFade / WHITEOUT_FADE_DURATION, 0.0f, 1.0f);
                    m_whiteAlpha = linearT * linearT * (3.0f - 2.0f * linearT);
                }
                else if (timeInFade < WHITEOUT_FADE_DURATION + WHITEOUT_HOLD_DURATION)
                {
                    m_whiteAlpha = 1.0f;
                }
                else
                {
                    // Fade back ke game normal
                    float fadeOutTime = timeInFade - (WHITEOUT_FADE_DURATION + WHITEOUT_HOLD_DURATION);
                    m_whiteAlpha = 1.0f - std::clamp(fadeOutTime / FADE_BACK_DURATION, 0.0f, 1.0f);

                    if (m_navi)
                    {
                        m_navi->SetPotionedState(true);

                        m_navi->StartAttackDelay(999.0f);
                    }

                    // Final cleanup pas layarnya udah 100% normal (White alpha 0)
                    if (m_whiteAlpha <= 0.0f)
                    {
                        m_whiteAlpha = 0.0f;
                        m_isBossCinematicActive = false;

                        // Player masih di-lock, Navi belum nyerang
                        if (!m_hasTriggeredPoisonDialogue)
                        {
                            StartPoisonDialogue();
                        }
                    }
                }
            }
        }
    }

    else // NORMAL GAMEPLAY CAMERA
    {
        // 1. Target the Player securely
        if (m_player)
        {
            CameraController::Instance().SetTarget(m_player->GetPosition());
            m_director->Update(elapsedTime, m_player->GetMovement()->GetPosition());
        }

        // 2. Furi style cinematic combat zoom 
        static float targetZoom{ 0.0f };
        static int   frameCounter{ 0 };
        static const Enemy* cachedClosestEnemy{ nullptr };

        if (frameCounter++ % 10 == 0)
        {
            if (m_enemyManager && m_player)
            {
                float closestDistSq{ 999999.0f };
                const DirectX::XMFLOAT3 pPos{ m_player->GetPosition() };
                const Enemy* currentClosest{ nullptr };

                for (const auto& enemy : m_enemyManager->GetEnemies())
                {
                    if (!enemy || !enemy->IsActive()) continue;

                    const DirectX::XMFLOAT3 ePos{ enemy->GetPosition() };
                    const float dx{ pPos.x - ePos.x };
                    const float dz{ pPos.z - ePos.z };
                    const float distSq{ (dx * dx) + (dz * dz) };

                    if (distSq < closestDistSq)
                    {
                        closestDistSq = distSq;
                        currentClosest = enemy.get();
                    }
                }

                cachedClosestEnemy = currentClosest;

                if (cachedClosestEnemy)
                {
                    constexpr float combatRadius{ 25.0f };
                    constexpr float maxZoomIn{ -8.0f };

                    const float dist{ std::sqrt(closestDistSq) };
                    const float intensity{ std::clamp(1.0f - (dist / combatRadius), 0.0f, 1.0f) };
                    targetZoom = maxZoomIn * intensity;
                }
                else
                {
                    targetZoom = 0.0f;
                }
            }
        }

        if (cachedClosestEnemy && !cachedClosestEnemy->IsActive())
        {
            cachedClosestEnemy = nullptr;
            targetZoom = 0.0f;
        }

        CameraController::Instance().SetDynamicZoomOffset(targetZoom);
    }

    if (m_player && m_enemyManager && m_dialogueBox && !m_dialogueBox->IsActive())
    {
        // Cek hanya kalau salah satu dialog belum pernah ke-trigger
        if (!m_hasTriggeredMushroomDialogue || !m_hasTriggeredTrackingDialogue)
        {
            const DirectX::XMFLOAT3 pPos = m_player->GetPosition();

            for (const auto& enemy : m_enemyManager->GetEnemies())
            {
                if (!enemy || !enemy->IsActive()) continue;

                const DirectX::XMFLOAT3 ePos = enemy->GetPosition();
                const float dx = pPos.x - ePos.x;
                const float dz = pPos.z - ePos.z;
                const float distSq = (dx * dx) + (dz * dz);

                if (distSq < 150.0f)
                {
                    if (!m_hasTriggeredMushroomDialogue && enemy->GetType() == EnemyType::MushroomNone)
                    {
                        m_hasTriggeredMushroomDialogue = true;
                        StartMushroomDialogue();
                        break;
                    }
                    else if (!m_hasTriggeredTrackingDialogue && enemy->GetAttackType() == AttackType::Tracking)
                    {
                        m_hasTriggeredTrackingDialogue = true;
                        StartTrackingDialogue();
                        break;
                    }
                }
            }
        }
    }

    if (m_isPoisonDialogueActive && m_dialogueBox && !m_dialogueBox->IsActive())
    {
        // Kotak dialog udah hilang karena player mencet Enter di teks terakhir.
        m_isPoisonDialogueActive = false;

        // Lepas lock input player biar bisa gerak
        if (m_player)
        {
            m_player->SetInputEnabled(true);
            m_player->SetAimLocked(false);
        }

        // Navi langsung nyerang player! (Kasih delay tipis 0.5 detik biar dramatis)
        if (m_navi)
        {
            m_navi->StartAttackDelay(0.5f);
        }

    }

    // Finally, commit all calculations to the actual CameraController
    CameraController::Instance().Update(elapsedTime);

    if (m_player)
    {
        m_uberParams.glitchStrength = m_player->GetDamageGlitchIntensity();
    }

    EffectManager::Instance().Update(elapsedTime);
}

void SceneGame::StartPlayerDeathSequence()
{
    if (m_isDying) return;
    m_isDying = true;
    m_deathTimer = 0.0f;

    //AudioManager::Instance().PlaySFX("Data/Sound/SE_Explosion.wav", 0.4f);

    // Redundancy: Ensure the player is fully hidden and disabled
    if (m_player)
    {
        m_player->SetInputEnabled(false);
        m_player->scale = { 0.0f, 0.0f, 0.0f };
    }
}

void SceneGame::StartNaviDefeatSequence()
{
    if (m_isNaviDefeatSequenceActive) return;

    m_isNaviDefeatSequenceActive = true;
    m_naviDefeatTimer = 0.0f;
    m_isNaviDefeatReadyForNextScene = false;

    AudioManager::Instance().FadeOutMusic(NAVI_DEFEAT_FADE_DURATION);

    if (m_player)
    {
        m_player->SetInputEnabled(false);
        m_player->SetAimLocked(true);
        m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
        m_player->GetProjectiles().clear();
    }
}

void SceneGame::StartIntroDialogueTest()
{
    m_hasIntroDialogueTestStarted = true;

    if (m_dialogueBox)
    {
        // Masukkan dialog per halaman/tekanan tombol
        std::vector<std::string> dialogPages = {
            u8"目を覚まして。戦いの時間が来たわ。\n「Space」で攻撃よ。遠くの敵は撃ち抜き、\n近づけばその刃で斬り裂くの。",
            u8"そして、よく覚えておいて。\nいずれそのキーは、敵の牙を弾き返す\n「Parry」の要にもなるわ。魂に刻み込んで。",
            u8"次は「Shift」を試して。\n風のように「Dash」して、敵の弾幕をすり抜けるのよ。\n\nさあ、あなたの力を見せて。"
        };

        m_dialogueBox->SetPosition(536.0f, 750.0f);
        m_dialogueBox->StartDialogue(dialogPages);
    }
}

void SceneGame::StartMushroomDialogue()
{
    if (m_dialogueBox)
    {
        std::vector<std::string> dialogPages = {
            u8"あのキノコを見て。今は大人しく見えるけれど…\n気を抜かないで。",
            u8"この森の奥は奇妙な薬液で汚染されているわ。\n凶暴化した個体もいるはずよ。"
        };

        m_dialogueBox->StartDialogue(dialogPages);
    }
}

void SceneGame::StartTrackingDialogue()
{
    if (m_dialogueBox)
    {
        std::vector<std::string> dialogPages = {
            u8"危ない！あのキノコは他と違うわ！\nあなたを狙って自爆する気よ！",
            u8"近づかれる前に早く撃ち落として！"
        };

        m_dialogueBox->StartDialogue(dialogPages);
    }
}

void SceneGame::StartPoisonDialogue()
{
    if (m_dialogueBox)
    {
        std::vector<std::string> dialogPages = {
            u8"あ....あ、ぁ....",
            u8"あつい....からだが...とける....",
            u8"にげて...わたし、もう.....",
            u8"あはッ......アはハハハハハハハッ！！！！"
        };

        m_hasTriggeredPoisonDialogue = true;
        m_isPoisonDialogueActive = true;

        m_dialogueBox->StartDialogue(dialogPages);
    }
}
void SceneGame::ResetLevel()
{
    const bool isBossStage = m_bossCinematicTriggered;

    // 1. Calculate Respawn Position
    DirectX::XMFLOAT3 respawnPos = m_playerSpawnPos;
    if (isBossStage && m_stage && !m_stage->m_linesEnable.empty())
    {
        const auto& line = m_stage->m_linesEnable[0];
        respawnPos = { line.Position.x, m_playerSpawnPos.y, line.Position.z };
    }

    // 2. Reset Player State
    if (m_player)
    {
        m_player->SetPosition(respawnPos);
        m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
        m_player->SetMaxHP(isBossStage ? 150 : 100);
        m_player->SetInputEnabled(false);
        m_player->scale = { 1.0f, 1.0f, 1.0f };
        m_player->GetStateMachine()->ChangeState(m_player.get(), std::make_unique<PlayerIdle>());
        m_player->GetProjectiles().clear();
    }

    // 3. Reset Enemies & Items
    if (!isBossStage)
    {
        if (m_enemyManager)
        {
            m_enemyManager->GetEnemies().clear();
            m_enemyManager->Initialize(Graphics::Instance().GetDevice());
        }
        if (m_itemManager)
        {
            m_itemManager->GetItems().clear();
            m_itemManager->Initialize(Graphics::Instance().GetDevice());
        }
    }

    // 4. Reset Navi Ally
    if (m_navi)
    {
        m_navi->Reset();
        m_navi->SetPotionedState(isBossStage);

        // Position Navi near Player
        m_navi->SetPosition({ respawnPos.x + 1.0f, respawnPos.y + 2.0f, respawnPos.z + 0.5f });

        // --- NEW: TRIGGER DELAY ON RESPAWN ---
        if (isBossStage)
        {
            m_navi->StartAttackDelay(3.0f);
        }
    }

    // 5. Smart Camera Reset
    CameraController::Instance().SetDynamicZoomOffset(0.0f);
    CameraController::Instance().SetTarget(respawnPos);
    for (int i = 0; i < 60; ++i)
    {
        CameraController::Instance().Update(0.016f);
    }
}

void SceneGame::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam{ camera ? camera : m_mainCamera.get() };
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto rs{ Graphics::Instance().GetRenderState() };

    m_postProcess->SetEnabled(m_fxState.MasterEnabled);

    UberShader::UberData& activeData{ m_postProcess->GetData() };
    activeData = this->m_uberParams;

    activeData.psxEnabled = (m_fxState.MasterEnabled && m_fxState.EnablePSX);

    if (!m_fxState.EnableVignette && !m_isDying && !m_isNaviDefeatSequenceActive && m_respawnTimer <= 0.0f)
    {
        activeData.intensity = 0.0f;
    }
    if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; }
    if (!m_fxState.EnableChromatic) activeData.chromaticAberration = 0.0f;
    if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }
    if (!m_fxState.EnableBloom)     activeData.bloomIntensity = 0.0f;

    if (m_fxState.MasterEnabled) {
        m_postProcess->BeginCapture();
    }
    else {
        // Fallback clear if post-process is bypassed
        ID3D11RenderTargetView* originalRTV{ nullptr };
        ID3D11DepthStencilView* originalDSV{ nullptr };
        dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);
        if (originalRTV) {
            float clearColor[4]{ 0.0f, 0.0f, 0.0f, 1.0f }; // Solid black
            dc->ClearRenderTargetView(originalRTV, clearColor);
            originalRTV->Release();
        }
        if (originalDSV) {
            dc->ClearDepthStencilView(originalDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            originalDSV->Release();
        }
    }

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    if (targetCam == m_mainCamera.get()) {
        auto shapeRenderer{ Graphics::Instance().GetShapeRenderer() };
        auto primRenderer{ Graphics::Instance().GetPrimitiveRenderer() };

        //primRenderer->DrawGrid(50, 1.0f);

        if (m_itemManager) m_itemManager->RenderDebug(shapeRenderer);
        if (m_stage) m_stage->RenderDebug(shapeRenderer, primRenderer);
        if (m_enemyManager) m_enemyManager->RenderDebug(shapeRenderer);

        // Player hitbox (green), Enemy hitboxes (red)
        //if (m_player)
        //{
        //    DirectX::XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
        //    constexpr float PLAYER_RADIUS = 0.25f;
        //    shapeRenderer->DrawSphere(pPos, PLAYER_RADIUS, { 0.0f, 1.0f, 0.0f, 1.0f });
        //}
        //if (m_enemyManager && m_collisionManager)
        //{
        //    for (const auto& enemy : m_enemyManager->GetEnemies())
        //    {
        //        if (!enemy || !enemy->IsActive()) continue;

        //        DirectX::XMFLOAT3 ePos = enemy->GetPosition();

        //        // Ask the collision manager how big this specific enemy's hitbox is
        //        float radius = m_collisionManager->GetEnemyPushRadius(enemy.get());

        //        shapeRenderer->DrawSphere(ePos, radius, { 1.0f, 0.0f, 0.0f, 0.5f });
        //    }
        //}

        // Navi hitboxes (blue)
        //if (m_navi) m_navi->RenderDebug(shapeRenderer);

        shapeRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection());
        primRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    if (m_fxState.MasterEnabled) {
        m_postProcess->EndCapture(elapsedTime);
    }

    if (m_dialogueBox)
    {
        m_dialogueBox->Render(dc);
    }

    if (m_fadeAlpha > 0.001f && m_fadeSprite)
    {
        // Get dynamic screen size
        float screenW{ Config::DEFAULT_SCREEN_W };
        float screenH{ Config::DEFAULT_SCREEN_H };
        if (auto window{ Framework::Instance()->GetMainWindow() }) {
            screenW = static_cast<float>(window->GetWidth());
            screenH = static_cast<float>(window->GetHeight());
        }

        // Enable 2D Transparency
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        // Draw the sprite over the whole screen.
        m_fadeSprite->Render(
            dc,
            0.0f, 0.0f, 0.0f,      // dx, dy, dz
            screenW, screenH,      // dw, dh
            0.0f, 0.0f,            // sx, sy (左上)
            1920.0f, 1080.0f,      // sw, sh (テクスチャサイズに合わせて適宜変更)
            0.0f,                  // angle
            0.0f, 0.0f, 0.0f, m_fadeAlpha // r, g, b, a
        );
    }

    if (m_whiteAlpha > 0.001f && m_whiteSprite)
    {
        float screenW{ Config::DEFAULT_SCREEN_W };
        float screenH{ Config::DEFAULT_SCREEN_H };
        if (auto window{ Framework::Instance()->GetMainWindow() }) {
            screenW = static_cast<float>(window->GetWidth());
            screenH = static_cast<float>(window->GetHeight());
        }

        // Enable 2D Transparency
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        // Draw the white sprite over the whole screen.
        m_whiteSprite->Render(
            dc,
            0.0f, 0.0f, 0.0f,      // dx, dy, dz
            screenW, screenH,      // dw, dh
            0.0f, 0.0f,            // sx, sy 
            1920.0f, 1080.0f,      // sw, sh (texture size)
            0.0f,                  // angle
            1.0f, 1.0f, 1.0f, m_whiteAlpha // Apply fading alpha
        );
    }
}


void SceneGame::RenderScene(const float elapsedTime, Camera* camera)
{
    if (!camera) return;
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto modelRenderer{ Graphics::Instance().GetModelRenderer() };
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, &m_lightManager };

    rc.psxEnabled = (m_fxState.MasterEnabled && m_fxState.EnablePSX);
    rc.psxResWidth = m_uberParams.psxResWidth;
    rc.psxResHeight = m_uberParams.psxResHeight;

    if (m_player)
    {
        modelRenderer->Draw(ShaderId::Phong, m_player->GetModel(), m_player->color);
        m_player->RenderWeapon(modelRenderer);
        m_player->RenderProjectiles(modelRenderer);
    }
    if (m_navi) {
        m_navi->Render(modelRenderer);
        m_navi->RenderProjectiles(modelRenderer);
    }
    if (m_enemyManager) m_enemyManager->Render(modelRenderer);
    if (m_itemManager) m_itemManager->Render(modelRenderer);
    if (m_stage)
    {
        m_stage->UpdateTransform();
        m_stage->Render(modelRenderer);
    }

    modelRenderer->Render(rc);

    EffectManager::Instance().Render(camera);
}

void SceneGame::DrawGUI()
{
    //GameBreakerGUI::Draw(this); 
}

void SceneGame::OnResize(int width, int height)
{
    if (height <= 0) height = 1;
    if (m_mainCamera) {
        m_mainCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(Config::CAM_FOV), static_cast<float>(width) / static_cast<float>(height), Config::CAM_NEAR, Config::CAM_FAR);
    }
    if (m_postProcess) m_postProcess->OnResize(width, height);
}

bool SceneGame::AreTrackingEnemiesDead() const
{
    if (!m_enemyManager) return false;

    // CPU Optimization: Range-based for loop.
    for (const auto& enemy : m_enemyManager->GetEnemies())
    {
        // If we find even ONE active tracking enemy, abort.
        if (enemy && enemy->IsActive() && enemy->GetAttackType() == AttackType::Tracking)
        {
            return false;
        }
    }
    return true;
}

Enemy* SceneGame::GetFakeBoss() const
{
    if (!m_enemyManager) return nullptr;

    for (const auto& enemy : m_enemyManager->GetEnemies())
    {
        if (enemy && enemy->IsActive() && enemy->GetType() == EnemyType::FakeBoss)
        {
            return enemy.get();
        }
    }
    return nullptr;
}

void SceneGame::StartBossCinematic()
{
    if (m_bossCinematicTriggered) return;

    Enemy* fakeBoss = GetFakeBoss();

    if (!fakeBoss || !m_player) return;

    m_bossCinematicTriggered = true;
    m_isBossCinematicActive = true;
    m_bossCinematicTimer = 0.0f;
    m_bossDialogueStarted = false;

    // Lock the Player securely
    m_player->SetInputEnabled(false);
    m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    m_player->GetStateMachine()->ChangeState(m_player.get(), std::make_unique<PlayerIdle>());
    m_player->SetAimLocked(true);
    m_player->ForceAimTarget(fakeBoss->GetPosition());

    // Set LERP anchors
    m_cinematicStartTarget = m_player->GetPosition();
    m_cinematicEndTarget = fakeBoss->GetPosition();
}
