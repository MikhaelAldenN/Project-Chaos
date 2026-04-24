#include <imgui.h>
#include <SDL3/SDL.h>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGame.h"
#include "WindowManager.h"

SceneGame::SceneGame()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;

    // 1. Initialize Main Assets
    mainCamera = std::make_shared<Camera>();
    mainCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(45), screenW / screenH, 0.1f, 1000.0f);
    mainCamera->SetPosition(0, 5, -5);
    mainCamera->LookAt({ 0, 0, 0 });

    CameraController::Instance().SetActiveCamera(mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::GamePad);

    player = new Player();

    // 2. Initialize Secondary Camera (CCTV)
    subCamera = new Camera();
    subCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 400.0f / 300.0f, 0.1f, 1000.0f);
    subCamera->SetPosition(5, 5, 5);
    subCamera->LookAt({ 0, 0, 0 });

    // 3. Initialize Windows
    trackingWindow = WindowManager::Instance().CreateGameWindow("Tracking View", 300, 300);
    trackingCamera = new Camera();
    trackingCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
    trackingWindow->SetCamera(trackingCamera);

    lensWindow = WindowManager::Instance().CreateGameWindow("Lens View (Drag Me!)", 300, 300);
    lensCamera = new Camera();
    lensCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
    lensCamera->SetRotation(90.0f, 0.0f, 0.0f); // Top-down
    lensWindow->SetCamera(lensCamera);
}

SceneGame::~SceneGame()
{
    CameraController::Instance().ClearCamera();
    if (subCamera) delete subCamera;
    if (player) delete player;

    for (Camera* cam : additionalCameras) delete cam;

    WindowManager::Instance().DestroyWindow(trackingWindow);
    if (trackingCamera) delete trackingCamera;

    WindowManager::Instance().DestroyWindow(lensWindow);
    if (lensCamera) delete lensCamera;
}

void SceneGame::Update(float elapsedTime)
{
    // Update Player & Camera Controller
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();
    if (player)
    {
        player->Update(elapsedTime, activeCam);
        CameraController::Instance().SetTarget(player->GetPosition());

        // Update CCTV Camera
        if (subCamera)
        {
            subCamera->SetPosition(5, 5.0f, 5);
            subCamera->LookAt(player->GetPosition());
        }
    }

    UpdateTrackingWindow();
    UpdateLensWindow();
    HandleDebugInput();

    CameraController::Instance().Update(elapsedTime);
}

void SceneGame::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = CameraController::Instance().GetActiveCamera().get();

    // Setup Render State
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // Render Debug Shapes (Main Camera Only)
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }
}

void SceneGame::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto primRenderer = Graphics::Instance().GetPrimitiveRenderer();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();

    // Draw Grid
    primRenderer->DrawGrid(20, 1);
    primRenderer->Render(dc, camera->GetView(), camera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // Draw Player
    if (player)
    {
        RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };
        player->Render(modelRenderer);
        modelRenderer->Render(rc);
    }
}

// --- Helper Functions ---

void SceneGame::UpdateTrackingWindow()
{
    if (!player || !trackingWindow || !trackingCamera) return;

    static float currentPixelRatio = 40.0f;
    DirectX::XMFLOAT3 pPos = player->GetPosition();

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Calculate Window Position
    float centerX = screenW / 2.0f;
    float centerY = screenH / 2.0f;
    float targetX = centerX + (pPos.x * currentPixelRatio);
    float targetY = centerY - (pPos.z * currentPixelRatio);

    float winW = (float)trackingWindow->GetWidth();
    float winH = (float)trackingWindow->GetHeight();

    // Snap to Integer pixels to prevent jitter
    int finalWinX = (int)(targetX - (winW / 2.0f) + 0.5f);
    int finalWinY = (int)(targetY - (winH / 2.0f) + 0.5f);

    SDL_SetWindowPosition(trackingWindow->GetSDLWindow(), finalWinX, finalWinY);

    // Calculate Off-Center Projection
    float fovY = DirectX::XM_PI / 3.0f;
    float halfFovTan = tanf(fovY / 2.0f);
    float camHeight = centerY / (currentPixelRatio * halfFovTan);

    trackingCamera->SetPosition(0.0f, camHeight, 0.0f);
    trackingCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    float nearZ = 0.1f, farZ = 1000.0f;
    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    float l = ((float)finalWinX / screenW) * 2.0f - 1.0f;
    float r = (((float)finalWinX + winW) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - ((float)finalWinY / screenH) * 2.0f;
    float b = 1.0f - (((float)finalWinY + winH) / screenH) * 2.0f;

    trackingCamera->SetOffCenterProjection(l * halfWidth, r * halfWidth, b * halfHeight, t * halfHeight, nearZ, farZ);
}

void SceneGame::UpdateLensWindow()
{
    if (!lensWindow || !lensCamera) return;

    float fixedPixelRatio = 40.0f;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    float fovY = DirectX::XM_PI / 3.0f;
    float halfFovTan = tanf(fovY / 2.0f);
    float camHeight = (screenH / 2.0f) / (fixedPixelRatio * halfFovTan);

    lensCamera->SetPosition(0.0f, camHeight, 0.0f);
    lensCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    int winX, winY;
    SDL_GetWindowPosition(lensWindow->GetSDLWindow(), &winX, &winY);
    int winW = lensWindow->GetWidth();
    int winH = lensWindow->GetHeight();

    float nearZ = 0.1f, farZ = 1000.0f;
    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    float l = ((float)winX / screenW) * 2.0f - 1.0f;
    float r = (((float)winX + winW) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - ((float)winY / screenH) * 2.0f;
    float b = 1.0f - (((float)winY + winH) / screenH) * 2.0f;

    lensCamera->SetOffCenterProjection(l * halfWidth, r * halfWidth, b * halfHeight, t * halfHeight, nearZ, farZ);
}

void SceneGame::HandleDebugInput()
{
    if (Input::Instance().GetKeyboard().IsTriggered('N'))
    {
        Beyond::Window* addWin = WindowManager::Instance().CreateGameWindow("AdditionalWin", 300, 300);
        Camera* addCam = new Camera();

        addCam->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
        if (player)
        {
            auto pPos = player->GetPosition();
            addCam->SetPosition(pPos.x + 5.0f, pPos.y + 5.0f, pPos.z - 5.0f);
            addCam->LookAt(pPos);
        }

        addWin->SetCamera(addCam);
        additionalCameras.push_back(addCam);
    }

    // Sync additional cameras with player
    if (player)
    {
        auto pPos = player->GetPosition();
        for (Camera* cam : additionalCameras)
        {
            cam->SetPosition(pPos.x + 5.0f, pPos.y + 5.0f, pPos.z - 5.0f);
            cam->LookAt(pPos);
        }
    }
}

void SceneGame::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();
    ImGui::Begin("Scene Info");
    ImGui::Text("WASD to Move");
    ImGui::End();
}

void SceneGame::OnResize(int width, int height)
{
    if (height == 0) height = 1;
    if (mainCamera)
    {
        mainCamera->SetAspectRatio((float)width / (float)height);
    }
}