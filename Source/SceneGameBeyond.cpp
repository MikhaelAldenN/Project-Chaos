#include <imgui.h>
#include <SDL3/SDL.h>
#include <cmath>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGameBeyond.h"
#include "WindowManager.h"
#include "Framework.h"

using namespace DirectX;

SceneGameBeyond::SceneGameBeyond()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;

    // 1. Setup Main Camera
    mainCamera = std::make_shared<Camera>();
    mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);

    // Calculate and apply unified height for 1:1 pixel mapping
    float unifiedHeight = GetUnifiedCameraHeight();
    mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f); // Z=0 for top-down alignment
    mainCamera->LookAt({ 0, 0, 0 });

    // Initialize Controller
    CameraController::Instance().SetActiveCamera(mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));
    CameraController::Instance().SetTarget(XMFLOAT3(0, 0, 0));

    // 2. Setup Player
    player = std::make_unique<Player>();
    player->SetInvertControls(true);

    // 2. Setup Block Manager (Companion System)
    blockManager = std::make_unique<BlockManager>();

    // Initialize dengan player agar referensinya tersambung
    blockManager->Initialize(player.get());

    blockManager->ClearBlocks();

    // Aktifkan Mode Formasi agar blok yang nanti dispawn langsung nempel ke player
    blockManager->ActivateFormationMode();

    boss = std::make_unique<Boss>();

    int allyCount = 20;
    for (int i = 0; i < allyCount; ++i)
    {
        blockManager->SpawnAllyBlock(player.get());
    }

    // 3. Setup Secondary Camera (CCTV/Debug)
    subCamera = std::make_shared<Camera>();
    subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    subCamera->SetPosition(5, 5, 5);
    subCamera->LookAt({ 0, 0, 0 });

    // Sub-windows are initialized in Update() to prevent startup freeze
    trackingWindow = nullptr;
    lensWindow = nullptr;
}

SceneGameBeyond::~SceneGameBeyond()
{
    // Clean up controller and request window destruction
    CameraController::Instance().ClearCamera();
    WindowManager::Instance().DestroyWindow(trackingWindow);
    WindowManager::Instance().DestroyWindow(lensWindow);
    additionalCameras.clear();
}

void SceneGameBeyond::InitializeSubWindows()
{
    // 1. Create Tracking Window (Priority 1: Below Lens)
    trackingWindow = WindowManager::Instance().CreateGameWindow("Tracking View", 300, 300);
    trackingWindow->SetPriority(1);

    trackingCamera = std::make_shared<Camera>();
    trackingWindow->SetCamera(trackingCamera.get());

    // 2. Create Lens Window (Priority 0: Topmost)
    lensWindow = WindowManager::Instance().CreateGameWindow("Lens View (Drag Me!)", 300, 300);
    lensWindow->SetPriority(0);

    lensCamera = std::make_shared<Camera>();
    lensCamera->SetRotation(90.0f, 0.0f, 0.0f); // Pure Top-down
    lensWindow->SetCamera(lensCamera.get());

    // -------------------------------------------------------------
    // [BARU] INIT POSISI AWAL (Supaya tidak snapping)
    // -------------------------------------------------------------
    if (player && trackingWindow)
    {
        // Hitung posisi target ideal saat ini (Logic sama seperti di Update)
        XMFLOAT3 pPos = player->GetPosition();
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        float targetX = (screenW / 2.0f) + (pPos.x * PIXEL_TO_UNIT_RATIO);
        float targetY = (screenH / 2.0f) - (pPos.z * PIXEL_TO_UNIT_RATIO);

        // Pusatkan
        float winW = (float)trackingWindow->GetWidth();
        float winH = (float)trackingWindow->GetHeight();

        // Simpan ke variabel float kita
        m_windowX = targetX - (winW / 2.0f);
        m_windowY = targetY - (winH / 2.0f);

        // Set langsung posisi awal
        SDL_SetWindowPosition(trackingWindow->GetSDLWindow(), (int)m_windowX, (int)m_windowY);
    }

    if (auto mainWin = Framework::Instance()->GetMainWindow())
    {
        // Menyembunyikan window utama agar game terlihat seperti
        // aplikasi desktop yang "bolong-bolong" (Transparent feel)
        mainWin->SetVisible(true);
    }

    // Apply priority immediately
    WindowManager::Instance().EnforceWindowPriorities();
    isWindowsInitialized = true;
}

void SceneGameBeyond::Update(float elapsedTime)
{

    // --- Deferred Initialization ---
    if (!isWindowsInitialized)
    {
        startupTimer += elapsedTime;
        if (startupTimer > DEFERRED_INIT_TIME)
            InitializeSubWindows();
        else
            return; // Skip logic until ready
    }

    // --- Main Logic ---
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    if (player)
    {
        player->Update(elapsedTime, activeCam);

        // Update CCTV to follow player
        if (subCamera)
        {
            subCamera->SetPosition(5, 5.0f, 5);
            subCamera->LookAt(player->GetPosition());
        }
    }

    if (blockManager)
    {
        blockManager->Update(elapsedTime, activeCam, player.get());

        // INPUT SPAWN: Tekan 'R' untuk memunculkan teman baru
        if (Input::Instance().GetKeyboard().IsTriggered('R'))
        {
            blockManager->SpawnAllyBlock(player.get());
        }

    }

    if (isWindowsInitialized)
    {
        // [UPDATE] Kirim elapsedTime
        UpdateTrackingLogic(elapsedTime);

        UpdateLensLogic();
        WindowManager::Instance().EnforceWindowPriorities();
    }

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}

void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : mainCamera.get();

    // Setup Render State
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // Only render Debug Shapes on main camera to save performance
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }
}

void SceneGameBeyond::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();

    // [FIX] Ambil Primitive Renderer untuk Grid
    auto primRenderer = Graphics::Instance().GetPrimitiveRenderer();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();

    // -------------------------------------------------------------------------
    // 1. RENDER GRID (KEMBALIKAN INI)
    // -------------------------------------------------------------------------
    // Kita gambar grid dulu agar ada di paling bawah/background
    primRenderer->DrawGrid(20, 1);
    primRenderer->Render(dc, camera->GetView(), camera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // -------------------------------------------------------------------------
    // 2. SETUP CULLING (OPTIMASI)
    // -------------------------------------------------------------------------
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    DirectX::XMFLOAT3 camPos = camera->GetPosition();
    float cullingRadius = 40.0f;

    // Helper Lambda untuk Cek Jarak
    auto IsVisible = [&](const DirectX::XMFLOAT3& objPos) -> bool {
        float dx = camPos.x - objPos.x;
        float dz = camPos.z - objPos.z;
        // Jarak kuadrat < Radius kuadrat (Lebih cepat daripada sqrt)
        return (dx * dx + dz * dz) < (cullingRadius * cullingRadius);
        };

    // -------------------------------------------------------------------------
    // 3. RENDER PLAYER
    // -------------------------------------------------------------------------
    if (player)
    {
        player->Render(modelRenderer);
        // Player kita render terpisah atau gabung batch nanti tidak masalah,
        // tapi pastikan dia digambar.
    }


    // -------------------------------------------------------------------------
    // 4. RENDER BLOCKS (DENGAN CULLING)
    // -------------------------------------------------------------------------
    if (blockManager)
    {
        // Loop manual untuk memilah mana block yang DEKAT kamera
        for (const auto& block : blockManager->GetBlocks())
        {
            if (!block->IsActive()) continue;

            // [OPTIMASI] Cek Jarak! Kalau jauh, SKIP render
            if (!IsVisible(block->GetMovement()->GetPosition())) continue;

            // Kalau dekat, masukkan antrian render
            block->Render(modelRenderer, blockManager->globalBlockColor);
        }
    }

    if (boss)
    {
        // Boss perlu update animasi tiap frame render (atau pindah ke Update() lebih baik)
        boss->Update(elapsedTime);
        boss->Render(modelRenderer);
    }

    // -------------------------------------------------------------------------
    // 5. FLUSH DRAW CALL (Kirim ke GPU)
    // -------------------------------------------------------------------------
    // Ini akan menggambar Player + Block yang lolos seleksi tadi sekaligus
    modelRenderer->Render(rc);
}

// =========================================================
// HELPER FUNCTIONS & LOGIC
// =========================================================

float SceneGameBeyond::GetUnifiedCameraHeight() const
{
    // Calculates height required so that 1 Unity Unit = PIXEL_TO_UNIT_RATIO pixels
    int sysScreenH = GetSystemMetrics(SM_CYSCREEN);
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f);
    return (sysScreenH / 2.0f) / (PIXEL_TO_UNIT_RATIO * halfFovTan);
}

// [UPDATE] Tambahkan parameter dt
void SceneGameBeyond::UpdateTrackingLogic(float dt)
{
    if (!player || !trackingWindow || !trackingCamera) return;

    // 1. Calculate Target (Sama seperti sebelumnya)
    XMFLOAT3 pPos = player->GetPosition();
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    float centerX = screenW / 2.0f;
    float centerY = screenH / 2.0f;
    float targetX = centerX + (pPos.x * PIXEL_TO_UNIT_RATIO);
    float targetY = centerY - (pPos.z * PIXEL_TO_UNIT_RATIO);

    float winW = (float)trackingWindow->GetWidth();
    float winH = (float)trackingWindow->GetHeight();

    // Target akhir (Centered)
    float destX = targetX - (winW / 2.0f);
    float destY = targetY - (winH / 2.0f);

    // -------------------------------------------------------------
    // [BARU] SMOOTH DAMPING (LERP)
    // -------------------------------------------------------------
    // Rumus: Current += (Target - Current) * Speed * Time

    // Faktor interpolasi (Clamp biar gak overshoot kalau fps drop)
    float t = m_windowFollowSpeed * dt;
    if (t > 1.0f) t = 1.0f;

    m_windowX += (destX - m_windowX) * t;
    m_windowY += (destY - m_windowY) * t;

    // -------------------------------------------------------------
    // 3. APPLY TO WINDOW
    // -------------------------------------------------------------
    // Kita cast ke int hanya saat set posisi, tapi simpan float-nya
    SDL_SetWindowPosition(trackingWindow->GetSDLWindow(), (int)m_windowX, (int)m_windowY);

    // 4. Update Camera Projection (Tetap sama)
    float camHeight = (centerY / (PIXEL_TO_UNIT_RATIO * tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f)));
    UpdateOffCenterProjection(trackingCamera.get(), trackingWindow, camHeight);
}

void SceneGameBeyond::UpdateLensLogic()
{
    if (!lensWindow || !lensCamera) return;

    // Lens uses the calculated unified height to match the main screen scale
    UpdateOffCenterProjection(lensCamera.get(), lensWindow, GetUnifiedCameraHeight());
}

void SceneGameBeyond::UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight)
{
    // --- CORE DRY FUNCTION ---
    // Calculates the off-center projection matrix so the small window 
    // acts as a "portal" or "lens" to the larger world.

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Set Camera Position
    targetCam->SetPosition(0.0f, camHeight, 0.0f);
    targetCam->LookAt({ 0.0f, 0.0f, 0.0f });

    // Get Current Window Rect
    int winX, winY;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);
    int winW = targetWin->GetWidth();
    int winH = targetWin->GetHeight();

    // Calculate Frustum Bounds
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    // Remap window coordinates (-1 to 1 space relative to full screen)
    float l = ((float)winX / screenW) * 2.0f - 1.0f;
    float r = (((float)winX + winW) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - ((float)winY / screenH) * 2.0f;
    float b = 1.0f - (((float)winY + winH) / screenH) * 2.0f;

    // Apply Projection
    targetCam->SetOffCenterProjection(l * halfWidth, r * halfWidth, b * halfHeight, t * halfHeight, nearZ, farZ);
}

void SceneGameBeyond::HandleDebugInput()
{
    if (Input::Instance().GetKeyboard().IsTriggered('N'))
    {
        GameWindow* addWin = WindowManager::Instance().CreateGameWindow("Debug Cam", 300, 300);
        auto addCam = std::make_shared<Camera>();

        addCam->SetPerspectiveFov(XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
        addWin->SetCamera(addCam.get());

        additionalCameras.push_back(addCam);
    }

    // Sync debug cameras with player
    if (player && !additionalCameras.empty())
    {
        XMFLOAT3 pPos = player->GetPosition();
        XMFLOAT3 offsetPos = { pPos.x + 5.0f, pPos.y + 5.0f, pPos.z - 5.0f };

        for (auto& cam : additionalCameras)
        {
            cam->SetPosition(offsetPos);
            cam->LookAt(pPos);
        }
    }
}

void SceneGameBeyond::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();

    ImGui::Begin("Scene Info");
    ImGui::Text("Controls: WASD (Move), N (New Window)");
    if (player)
    {
        player->DrawDebugGUI();
    }
    // Slider untuk mengatur kelicinan window
    // Nilai kecil (1.0 - 3.0) = Sangat Smooth/Lambat (Cinematic)
    // Nilai besar (10.0+) = Cepat/Responsif (Arcade)
    ImGui::DragFloat("Window Follow Speed", &m_windowFollowSpeed, 0.1f, 0.1f, 50.0f);
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