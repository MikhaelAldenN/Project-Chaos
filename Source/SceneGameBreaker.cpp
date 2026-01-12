#include <imgui.h>
#include <SDL3/SDL.h>
#include "Framework.h"
#include "SceneGameBreaker.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include "System/Input.h"

using namespace DirectX;

SceneGameBreaker::SceneGameBreaker()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // --------------------------------------------------------
    // Initialize Camera 
    // --------------------------------------------------------
    mainCamera = new Camera();
    mainCamera->SetPerspectiveFov(XMConvertToRadians(initialFOV), screenW / screenH, cameraNearZ, cameraFarZ);

    auto& camCtrl = CameraController::Instance();
    camCtrl.SetActiveCamera(mainCamera);
    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
    camCtrl.SetFixedSetting(cameraPosition);
    camCtrl.SetTarget(cameraTarget);

    // --------------------------------------------------------
    // Initialize Assets
    // --------------------------------------------------------
    m_backgroundSprite = std::make_unique<Sprite>(
        Graphics::Instance().GetDevice(),
        backgroundPath
    );
    m_bgRotation = 0.0f;
    m_bgPosition = { 0.0f, 0.0f };

    ball = new Ball();
    paddle = new Paddle();
    paddle->SetAIEnabled(false);
    player = new Player();
    player->SetInputEnabled(false);
    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player);
}

SceneGameBreaker::~SceneGameBreaker()
{
    if (mainCamera) delete mainCamera;
    if (player) delete player;
    if (paddle) delete paddle;
    if (ball) delete ball;
}

void SceneGameBreaker::Update(float elapsedTime)
{
    Camera* activeCam = CameraController::Instance().GetActiveCamera();

    // --------------------------------------------------------
    // HANDLE ANIMATION (TRANSITION)
    // --------------------------------------------------------
    if (m_isAnimating)
    {
        UpdateAnimation(elapsedTime);
    }

    // --------------------------------------------------------
    // Update Paddle & AI
    // --------------------------------------------------------
   
    if (paddle)
    {
        if (ball) paddle->UpdateAI(elapsedTime, ball, blockManager.get());
        paddle->Update(elapsedTime, activeCam);
    }

    // --------------------------------------------------------
    // Update Ball Logic
    // --------------------------------------------------------
    if (ball)
    {
        if (paddle && !ball->IsActive())
        {
            XMFLOAT3 padPos = paddle->GetPosition();
            padPos.z += ballSpawnZOffset;
            padPos.y = 0.0f;
            ball->GetMovement()->SetPosition(padPos);
        }
    
        ball->Update(elapsedTime, activeCam);
    
        // Paddle Collision
        if (paddle && ball->IsActive())
        {
            paddle->CheckCollision(ball);
        }
    
    }

    // --------------------------------------------------------
    // Update Block Manager
    // --------------------------------------------------------
    if (blockManager)
    {
        blockManager->Update(elapsedTime, activeCam);
        if (ball && ball->IsActive()) blockManager->CheckCollision(ball);
    }

    // --------------------------------------------------------
    // Update Player
    // --------------------------------------------------------
    if (player) player->Update(elapsedTime, activeCam);

    CameraController::Instance().Update(elapsedTime);
    UpdateGameTriggers(elapsedTime);
}

void SceneGameBreaker::UpdateGameTriggers(float elapsedTime)
{
    if (m_isAnimating || m_hasTriggered) return;

    // Check if blocks are low enough to trigger Breakout
    bool triggerCondition = (GetKeyState('T') & 0x8000) // For debug only delete later 
        || (blockManager && blockManager->GetActiveBlockCount() <= triggerBlockCount);

    if (triggerCondition)
    {
        m_isAnimating = true;
        m_animTimer = 0.0f;
    }
}

void SceneGameBreaker::UpdateAnimation(float elapsedTime)
{
    m_animTimer += elapsedTime;
    float t = (std::min)(m_animTimer / animDuration, 1.0f);
    float smoothT = t * t * (3.0f - 2.0f * t);

    // Camera Rotation (Existing code)
    float currentOffset = animCameraRotationTotal * smoothT;
    CameraController::Instance().SetFixedYawOffset(currentOffset);

    // Sprite Rotation
    m_bgRotation = animTargetBgRotation * smoothT;
    m_bgPosition = { 0.0f, 0.0f };
    
    // Finish Logic
    if (t >= 1.0f)
    {
        m_isAnimating = false;
        m_hasTriggered = true;

        // AI Takes over
        if (paddle)
        {
            paddle->SetAIEnabled(true);     // Enable Auto-Pilot
        }
        if (player)
        {
            player->SetBreakoutMode(true);
            player->SetInputEnabled(false); // Disable Keyboard control
        }

    }
}

void SceneGameBreaker::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : mainCamera;

    // Setup Render State
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // ---------------------------------------------------------
    // RENDER BACKGROUND (2D)
    // ---------------------------------------------------------
    float screenW = 1280.0f;
    float screenH = 720.0f;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // Set Render States for 2D Sprite (Transparency + No Depth Test)
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

    if (m_backgroundSprite)
    {
        m_backgroundSprite->Render(
            dc,
            0, 0,                           // X, Y
            0,                              // Z
            screenW, screenH,               // Fit to Screen
            m_bgRotation,                   // Angle
            bgSpriteColor.x, bgSpriteColor.y, bgSpriteColor.z, bgSpriteColor.w 
        );
    }

    // ---------------------------------------------------------
    // RENDER 3D SCENE
    // ---------------------------------------------------------

    // Restore Render States for 3D
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // Render Debug Shapes (Main Camera Only)
    if (targetCam == mainCamera)
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }
}

void SceneGameBreaker::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    if (ball) ball->Render(modelRenderer);
    if (blockManager) blockManager->Render(modelRenderer);
    if (paddle) paddle->Render(modelRenderer);
    if (player) player->Render(modelRenderer);

    modelRenderer->Render(rc);
}

void SceneGameBreaker::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();
    ImGui::Begin("Scene Info");
    ImGui::End();
}

void SceneGameBreaker::OnResize(int width, int height)
{
    if (height == 0) height = 1;
    if (mainCamera)
    {
        mainCamera->SetAspectRatio((float)width / (float)height);
    }
}