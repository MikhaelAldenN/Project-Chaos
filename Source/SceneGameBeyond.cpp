#include <imgui.h>
#include <SDL3/SDL.h>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGameBeyond.h"
#include "WindowManager.h"

SceneGameBeyond::SceneGameBeyond()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;

    // -----------------------------------------------------------
    // 1. Setup Main Camera dengan FOV 60 (Sama dengan window lain)
    // -----------------------------------------------------------
    mainCamera = std::make_shared<Camera>();
    // UBAH DARI 45 KE 60:
    mainCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenW / screenH, 0.1f, 1000.0f);

    // Inisialisasi awal (nanti akan ditimpa Controller)
    mainCamera->SetPosition(0, 5, 0);
    mainCamera->LookAt({ 0, 0, 0 });

    CameraController::Instance().SetActiveCamera(mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);

    // -----------------------------------------------------------
    // 2. Hitung "Perfect Height" (Logika sama dengan LensWindow)
    // -----------------------------------------------------------
    // Ambil tinggi layar monitor asli (bukan window game)
    int sysScreenH = GetSystemMetrics(SM_CYSCREEN);

    float pixelRatio = 40.0f; // Nilai yang sama dengan tracking/lens window
    float fovRad = DirectX::XMConvertToRadians(60); // 60 Derajat
    float halfFovTan = tanf(fovRad / 2.0f);

    // Rumus: Height = (Layar/2) / (Ratio * tan(fov/2))
    float unifiedHeight = (sysScreenH / 2.0f) / (pixelRatio * halfFovTan);

    // -----------------------------------------------------------
    // 3. Masukkan Height ke CameraController
    // -----------------------------------------------------------
    // X = 0, Y = unifiedHeight, Z = -10 (mundur sedikit biar tidak tegak lurus banget, atau 0 jika mau top-down)
    CameraController::Instance().SetFixedSetting(DirectX::XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // Pastikan camera melihat ke titik tengah (atau player)
    CameraController::Instance().SetTarget(DirectX::XMFLOAT3(0, 0, 0));

    player = std::make_unique<Player>();

    player->SetInvertControls(true);

    // 2. Initialize Secondary Camera (CCTV)
    subCamera = std::make_shared<Camera>();
    subCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 400.0f / 300.0f, 0.1f, 1000.0f);
    subCamera->SetPosition(5, 5, 5);
    subCamera->LookAt({ 0, 0, 0 });

    //// 3. Initialize Windows
    //trackingWindow = WindowManager::Instance().CreateGameWindow("Tracking View", 300, 300);
    //trackingCamera = std::make_shared<Camera>();
    //trackingCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);

    //// Windowには生ポインタを渡す (.get())
    //trackingWindow->SetCamera(trackingCamera.get());

    //lensWindow = WindowManager::Instance().CreateGameWindow("Lens View (Drag Me!)", 300, 300);
    //lensCamera = std::make_shared<Camera>();
    //lensCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
    //lensCamera->SetRotation(90.0f, 0.0f, 0.0f); // Top-down
    //lensWindow->SetCamera(lensCamera.get());
    trackingWindow = nullptr;
    lensWindow = nullptr;

}

void SceneGameBeyond::InitializeSubWindows()
{
    // 1. Initialize Tracking Window
    trackingWindow = WindowManager::Instance().CreateGameWindow("Tracking View", 300, 300);
    trackingCamera = std::make_shared<Camera>();
    trackingCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
    trackingWindow->SetCamera(trackingCamera.get());

    // 2. Initialize Lens Window
    lensWindow = WindowManager::Instance().CreateGameWindow("Lens View (Drag Me!)", 300, 300);
    lensCamera = std::make_shared<Camera>();
    lensCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
    lensCamera->SetRotation(90.0f, 0.0f, 0.0f);
    lensWindow->SetCamera(lensCamera.get());

    areWindowsInitialized = true;
}

SceneGameBeyond::~SceneGameBeyond()
{
    // [重要] スマートポインタにしたので delete は不要です！
    // WindowManagerへの破棄リクエストと、Controllerのクリアだけ行います。

    CameraController::Instance().ClearCamera();

    // WindowManagerがWindowのポインタを管理しているならDestroyを呼ぶ
    WindowManager::Instance().DestroyWindow(trackingWindow);
    WindowManager::Instance().DestroyWindow(lensWindow);

    // vector内のカメラもshared_ptrなら自動解放されます
    additionalCameras.clear();
}

void SceneGameBeyond::Update(float elapsedTime)
{
    // -----------------------------------------------------------
    // [BARU] LOGIKA DEFERRED INITIALIZATION
    // Tunggu sebentar (misal 0.2 detik) sebelum membuat window anak
    // agar Main Window & Input System stabil dulu.
    // -----------------------------------------------------------
    if (!areWindowsInitialized)
    {
        startupTimer += elapsedTime;
        if (startupTimer > 0.2f)
        {
            InitializeSubWindows();
        }
        else
        {
            // Jangan jalankan update logic lain kalau belum siap
            return;
        }
    }

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    if (player)
    {
        player->Update(elapsedTime, activeCam);

        if (subCamera)
        {
            subCamera->SetPosition(5, 5.0f, 5);
            subCamera->LookAt(player->GetPosition());
        }
    }

    // Pastikan dicek window sudah ada atau belum
    if (areWindowsInitialized)
    {
        UpdateTrackingWindow();
        UpdateLensWindow();
    }

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}

void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    // 引数がnullなら自分のmainCameraを使う (.get()で生ポインタ化)
    Camera* targetCam = camera ? camera : mainCamera.get();

    // Setup Render State
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // エラー修正: ポインタ同士の比較 (.get()同士ならOK)
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }
}

void SceneGameBeyond::RenderScene(float elapsedTime, Camera* camera)
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

void SceneGameBeyond::UpdateTrackingWindow()
{
    if (!player || !trackingWindow || !trackingCamera) return;

    static float currentPixelRatio = 40.0f;

    static int lastWinX = -9999;
    static int lastWinY = -9999;

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

    if (finalWinX != lastWinX || finalWinY != lastWinY)
    {
        SDL_SetWindowPosition(trackingWindow->GetSDLWindow(), finalWinX, finalWinY);
        lastWinX = finalWinX;
        lastWinY = finalWinY;
    }

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

void SceneGameBeyond::UpdateLensWindow()
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

void SceneGameBeyond::HandleDebugInput()
{
    if (Input::Instance().GetKeyboard().IsTriggered('N'))
    {
        GameWindow* addWin = WindowManager::Instance().CreateGameWindow("AdditionalWin", 300, 300);

        // make_shared で作成
        auto addCam = std::make_shared<Camera>();

        addCam->SetPerspectiveFov(DirectX::XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
        if (player)
        {
            auto pPos = player->GetPosition();
            addCam->SetPosition(pPos.x + 5.0f, pPos.y + 5.0f, pPos.z - 5.0f);
            addCam->LookAt(pPos);
        }

        addWin->SetCamera(addCam.get());
        additionalCameras.push_back(addCam);
    }

    // Sync additional cameras with player
    if (player)
    {
        auto pPos = player->GetPosition();
        // shared_ptr なので範囲for文でOK
        for (auto& cam : additionalCameras)
        {
            cam->SetPosition(pPos.x + 5.0f, pPos.y + 5.0f, pPos.z - 5.0f);
            cam->LookAt(pPos);
        }
    }
}

void SceneGameBeyond::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();
    ImGui::Begin("Scene Info");
    ImGui::Text("WASD to Move");
    ImGui::End();
}

void SceneGameBeyond::OnResize(int width, int height)
{
    if (height == 0) height = 1;
    if (mainCamera)
    {
        mainCamera->SetAspectRatio((float)width / (float)height);
    }
}