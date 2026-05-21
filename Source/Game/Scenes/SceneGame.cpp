#include "SceneGame.h" 

#include "CameraController.h"
#include "CinematicDirector.h" 
#include "Framework.h"
#include "GUISceneGameBreaker.h" 
#include "PostProcessManager.h"
#include "System/AudioManager.h"
#include "System/CollisionManager.h"
#include "System/Graphics.h"
#include "InputHelper.h"

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

SceneGame::SceneGame()
{
    float screenW{ Config::DEFAULT_SCREEN_W };
    float screenH{ Config::DEFAULT_SCREEN_H };
    if (auto window{ Framework::Instance()->GetMainWindow() }) {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
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

    m_fadeSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Game/Black.png");
    m_whiteSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Game/White.png");
    EffectManager::Instance().PreloadEffect("Data/Effect/Hit.efk");
    EffectManager::Instance().PreloadEffect("Data/Effect/FakeBossPoison.efk");
}

SceneGame::~SceneGame()
{
    AudioManager::Instance().StopMusic();
    CameraController::Instance().ClearCamera();

    m_player.reset();
    m_stage.reset();
    m_enemyManager.reset();
    m_itemManager.reset();
}

void SceneGame::Update(const float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > Config::TIME_LOOP_MAX) m_globalTime -= Config::TIME_LOOP_MAX;

    if (m_player && m_player->GetHP() <= 0 && !m_isDying && m_respawnTimer <= 0.0f)
    {
        StartPlayerDeathSequence();
    }

    if (m_bootTimer > 0.0f)
    {
        m_bootTimer -= elapsedTime;
        m_fadeAlpha = 1.0f;
        m_uberParams.smoothness = FX_BLACK_SMOOTHNESS;
        m_uberParams.intensity = FX_BLACK_INTENSITY;

        if (m_player)
        {
            CameraController::Instance().SetTarget(m_player->GetPosition());
            CameraController::Instance().Update(0.0f); 
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

        // Quadratic Ease-Out for a smoother fade-in curve
        const float linearT{ std::clamp(m_respawnTimer / RESPAWN_FADE_DURATION, 0.0f, 1.0f) };
        const float t{ linearT * linearT };

        m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
        m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;
        m_fadeAlpha = t;
    }
    else
    {
        // Normal Gameplay Lighting
        m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
        m_fadeAlpha = 0.0f;

        if (!m_hasBGMStarted)
        {
            AudioManager::Instance().PlayMusic("Data/Sound/BGM_Game.wav", true);
            m_hasBGMStarted = true; 
        }
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
        bool canAttack{ true };
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
        m_bossCinematicTimer += elapsedTime;
        float t = std::clamp(m_bossCinematicTimer / BOSS_CINEMATIC_DURATION, 0.0f, 1.0f);

        // Zero-Cost Math: Smoothstep (Creates a buttery-smooth ease-in and ease-out)
        float smoothT = t * t * (3.0f - 2.0f * t);

        DirectX::XMFLOAT3 currentTarget = {
            m_cinematicStartTarget.x + (m_cinematicEndTarget.x - m_cinematicStartTarget.x) * smoothT,
            m_cinematicStartTarget.y + (m_cinematicEndTarget.y - m_cinematicStartTarget.y) * smoothT,
            m_cinematicStartTarget.z + (m_cinematicEndTarget.z - m_cinematicStartTarget.z) * smoothT
        };

        // Force zoom out to the base Furi style view during cinematic
        CameraController::Instance().SetDynamicZoomOffset(0.0f);
        CameraController::Instance().SetTarget(currentTarget);

        if (!m_bossEffectTriggered && m_bossCinematicTimer >= (BOSS_CINEMATIC_DURATION + BOSS_CINEMATIC_HOLD_DURATION))
        {
            m_bossEffectTriggered = true;

            if (Enemy* fakeBoss = GetFakeBoss())
            {
                DirectX::XMFLOAT3 spawnPos = fakeBoss->GetPosition();

                spawnPos.x += m_fakeBossEffectOffset.x;
                spawnPos.y += m_fakeBossEffectOffset.y;
                spawnPos.z += m_fakeBossEffectOffset.z;

                // 1. Save the lightweight Handle
                Effekseer::Handle effHandle = EffectManager::Instance().Play(
                    "Data/Effect/FakeBossPoison.efk",
                    spawnPos,
                    m_fakeBossEffectScale
                );

                // 2. Apply Rotation (Convert GUI Degrees to Effekseer Radians)
                if (effHandle >= 0)
                {
                    DirectX::XMFLOAT3 rotRad{
                        DirectX::XMConvertToRadians(m_fakeBossEffectRotation.x),
                        DirectX::XMConvertToRadians(m_fakeBossEffectRotation.y),
                        DirectX::XMConvertToRadians(m_fakeBossEffectRotation.z)
                    };
                    EffectManager::Instance().SetRotation(effHandle, rotRad);
                }
            }
        }

        const float whiteoutStartTime{ BOSS_CINEMATIC_DURATION + BOSS_CINEMATIC_HOLD_DURATION + BOSS_EFFECT_WHITEOUT_DELAY };
        const float holdStartTime{ whiteoutStartTime + WHITEOUT_FADE_DURATION };
        const float fadeBackStartTime{ holdStartTime + WHITEOUT_HOLD_DURATION };

        // 1. PHASE: DROP CURTAIN
        if (m_bossCinematicTimer >= whiteoutStartTime && m_bossCinematicTimer < holdStartTime)
        {
            const float timeInFade{ m_bossCinematicTimer - whiteoutStartTime };
            const float linearT{ std::clamp(timeInFade / WHITEOUT_FADE_DURATION, 0.0f, 1.0f) };
            // Smoothstep
            m_whiteAlpha = linearT * linearT * (3.0f - 2.0f * linearT);
        }
        // 2. PHASE: HOLD (Force 1.0)
        else if (m_bossCinematicTimer >= holdStartTime && m_bossCinematicTimer < fadeBackStartTime)
        {
            m_whiteAlpha = 1.0f;
        }
        // 3. PHASE: FADE BACK
        else if (m_bossCinematicTimer >= fadeBackStartTime)
        {
            const float fadeOutTime{ m_bossCinematicTimer - fadeBackStartTime };
            m_whiteAlpha = 1.0f - std::clamp(fadeOutTime / FADE_BACK_DURATION, 0.0f, 1.0f);

            if (m_navi) m_navi->SetPotionedState(true);

            // Resume gameplay once fading starts
            if (m_player)
            {
                m_player->SetInputEnabled(true);
                m_player->SetAimLocked(false); 
            }

            // Final cleanup
            if (m_whiteAlpha <= 0.0f)
            {
                m_whiteAlpha = 0.0f;
                m_isBossCinematicActive = false;
            }
        }
    }
    else // NORMAL GAMEPLAY CAMERA
    {
        // 1. Target the Player securely
        if (m_player && m_fadeAlpha < 0.99f)
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

void SceneGame::ResetLevel()
{
    // 1. Reset Player State (Zero allocation)
    if (m_player)
    {
        m_player->SetPosition(m_playerSpawnPos); // Updates PhysX automatically
        m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
        m_player->SetMaxHP(100);
        m_player->SetInputEnabled(true);
        m_player->scale = { 1.0f, 1.0f, 1.0f };
        m_player->GetStateMachine()->ChangeState(m_player.get(), std::make_unique<PlayerIdle>());

        // Clean up any bullets the player shot right before dying
        m_player->GetProjectiles().clear();
    }

    // 2. Reset Enemies (Safe Object Pool Wipe)
    if (m_enemyManager)
    {
        m_enemyManager->GetEnemies().clear();
        m_enemyManager->Initialize(Graphics::Instance().GetDevice());
    }

    // 3. Reset Items
    if (m_itemManager)
    {
        m_itemManager->GetItems().clear();
        m_itemManager->Initialize(Graphics::Instance().GetDevice());
    }

    // 4. Reset Navi Ally
    if (m_navi)
    {
        m_navi->GetProjectiles().clear();

        // Snap navi back to the player's shoulder instantly
        DirectX::XMFLOAT3 naviPos{ m_playerSpawnPos };
        naviPos.x += 1.0f;
        naviPos.y += 2.0f; // HOVER_HEIGHT
        naviPos.z += 0.5f;
        m_navi->SetPosition(naviPos);
    }

	// 5. Reset Camera
    if (m_mainCamera)
    {
        XMFLOAT3 resetPos{ m_cameraPosition };
        resetPos.x = 0.0f;
        resetPos.z = -14.0f;
        resetPos.y = Config::CAM_START_HEIGHT;

        m_mainCamera->SetPosition(resetPos);
        m_mainCamera->LookAt(m_cameraTarget);

        // Force the CameraController to snap to the new position
        CameraController::Instance().SetFixedSetting(resetPos);
        CameraController::Instance().SetTarget(m_playerSpawnPos);
        CameraController::Instance().Update(0.0f); 
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

    if (!m_fxState.EnableVignette && !m_isDying && m_respawnTimer <= 0.0f)  
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

void SceneGame::DrawGUI() { GameBreakerGUI::Draw(this); }

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