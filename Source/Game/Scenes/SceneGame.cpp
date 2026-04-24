#include "SceneGame.h" 

#include "CameraController.h"
#include "CinematicDirector.h" 
#include "Framework.h"
#include "GUISceneGameBreaker.h" 
#include "PostProcessManager.h"
#include "System/CollisionManager.h"
#include "System/Graphics.h"

// Game Objects
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Player.h"
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
    startPos.z = 0.0f;
    startPos.y = Config::CAM_START_HEIGHT;

    m_mainCamera->SetPosition(startPos);
    m_mainCamera->LookAt(m_cameraTarget);
    camCtrl.SetActiveCamera(m_mainCamera);
    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
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

    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get());

    m_player->SetMoveSpeed(15.0f);
    m_player->SetInputEnabled(true);
    m_player->GetMovement()->SetRotationY(DirectX::XM_PI);

    m_enemyManager = std::make_unique<EnemyManager>();
    m_enemyManager->Initialize(Graphics::Instance().GetDevice());

    m_itemManager = std::make_unique<ItemManager>();
    m_itemManager->Initialize(Graphics::Instance().GetDevice());

    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), m_enemyManager.get(), m_itemManager.get());

    m_director = std::make_unique<CinematicDirector>();

    m_postProcess = std::make_unique<PostProcessManager>();
    m_postProcess->Initialize(static_cast<int>(screenW), static_cast<int>(screenH));
}

SceneGame::~SceneGame()
{
    CameraController::Instance().ClearCamera();
    m_player.reset();
}

void SceneGame::Update(const float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > Config::TIME_LOOP_MAX) m_globalTime -= Config::TIME_LOOP_MAX;

    if (m_scene) {
        m_scene->simulate(elapsedTime);
        m_scene->fetchResults(true);
    }

    Camera* activeCam{ CameraController::Instance().GetActiveCamera().get() };

    if (m_player) {
        m_player->Update(elapsedTime, activeCam);
        CameraController::Instance().SetTarget(m_player->GetPosition());
        m_director->Update(elapsedTime, m_player->GetMovement()->GetPosition());
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

    CameraController::Instance().Update(elapsedTime);

    m_uberParams.fineOpacity = 1.0f;
    m_uberParams.fineDensity = m_configFineDensity;
    m_uberParams.fineRotation = 0.0f;
    m_uberParams.scanlineStrength = Config::FX_CRT_BASE_STRENGTH;
    m_uberParams.glitchStrength = 0.0f;
    m_uberParams.distortion = 0.0f;
    m_uberParams.chromaticAberration = 0.0f;
    m_uberParams.smoothness = 0.2f;
    m_uberParams.intensity = 0.38f;
}

void SceneGame::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam{ camera ? camera : m_mainCamera.get() };
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto rs{ Graphics::Instance().GetRenderState() };

    m_postProcess->SetEnabled(m_fxState.MasterEnabled);

    if (m_fxState.MasterEnabled) {
        m_postProcess->BeginCapture();
    }
    else {
        ID3D11RenderTargetView* originalRTV{ nullptr };
        ID3D11DepthStencilView* originalDSV{ nullptr };
        dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);
        if (originalRTV) {
            float clearColor[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
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

        primRenderer->DrawGrid(50, 1.0f);

        if (m_itemManager) m_itemManager->RenderDebug(shapeRenderer);
        if (m_stage) m_stage->RenderDebug(shapeRenderer, primRenderer);
        if (m_enemyManager) m_enemyManager->RenderDebug(shapeRenderer);

        shapeRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection());
        primRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    if (m_fxState.MasterEnabled) {
        UberShader::UberData& activeData{ m_postProcess->GetData() };
        activeData = this->m_uberParams;

        if (!m_fxState.EnableVignette) activeData.intensity = 0.0f;
        if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; }
        if (!m_fxState.EnableChromatic) { activeData.chromaticAberration = 0.0f; }
        if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }

        m_postProcess->EndCapture(elapsedTime);
    }
}

void SceneGame::RenderScene(const float elapsedTime, Camera* camera)
{
    if (!camera) return;
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto modelRenderer{ Graphics::Instance().GetModelRenderer() };
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, &m_lightManager };

    if (m_player) modelRenderer->Draw(ShaderId::Phong, m_player->GetModel(), m_player->color);
    if (m_enemyManager) m_enemyManager->Render(modelRenderer);
    if (m_itemManager) m_itemManager->Render(modelRenderer);
    if (m_stage) {
        m_stage->UpdateTransform();
        m_stage->Render(modelRenderer);
    }

    modelRenderer->Render(rc);
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