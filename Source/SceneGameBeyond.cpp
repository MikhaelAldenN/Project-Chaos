#include <imgui.h>
#include <SDL3/SDL.h>
#include <cmath>
#include <windows.h> // GetSystemMetrics
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
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);

    float unifiedHeight = GetUnifiedCameraHeight();
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0, 0, 0 });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 2. Setup Assets
    m_player = std::make_unique<Player>();
    m_player->SetPosition(0.0f, 0.0f, 0.0f);
    m_player->SetInvertControls(true);

    m_blockManager = std::make_unique<BlockManager>();
    m_blockManager->Initialize(m_player.get());
    m_blockManager->ClearBlocks();
    m_blockManager->ActivateFormationMode();

    for (int i = 0; i < 20; ++i) m_blockManager->SpawnAllyBlock(m_player.get());

    m_boss = std::make_unique<Boss>();
    //m_boss->SetPosition(0.0f, 0.6f, 6.5f);
    //m_boss->SetRotation(65.0f, 0.0f, 0.0f);
    //m_boss->SetScale(10.0f, 10.0f, 10.0f);
    //m_boss->SetFloatingConfig(true, 2.0f, 0.2f, { 0.0f, 0.0f, 1.0f });

    m_player->SetPosition(0.0f, 0.0f, -8.0f);

    // 3. Sub Camera
    m_subCamera = std::make_shared<Camera>();
    m_subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    m_subCamera->SetPosition(5, 5, 5);
    m_subCamera->LookAt({ 0, 0, 0 });

    // 4. Setup Primitive Renderer for Fake Shatter
    auto device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
}

SceneGameBeyond::~SceneGameBeyond()
{
    WindowShatterManager::Instance().Clear();
    CameraController::Instance().ClearCamera();
    WindowManager::Instance().DestroyWindow(m_trackingWindow);
    WindowManager::Instance().DestroyWindow(m_lensWindow);
    WindowManager::Instance().DestroyWindow(m_bossWindow);
    WindowManager::Instance().DestroyWindow(m_cpuWindow);
    WindowManager::Instance().DestroyWindow(m_monitor2Window);
}

void SceneGameBeyond::InitializeSubWindows()
{
    // 1. Player Tracking Window
    m_trackingWindow = WindowManager::Instance().CreateGameWindow("Player View", 300, 300);
    m_trackingWindow->SetPriority(1);
    m_trackingWindow->SetDraggable(false);
    m_trackingCamera = std::make_shared<Camera>();
    m_trackingWindow->SetCamera(m_trackingCamera.get());

    //// 2. Lens Window
    //m_lensWindow = WindowManager::Instance().CreateGameWindow("Lens View", 300, 300);
    //m_lensWindow->SetPriority(0);
    //m_lensCamera = std::make_shared<Camera>();
    //m_lensCamera->SetRotation(90.0f, 0.0f, 0.0f);
    //m_lensWindow->SetCamera(m_lensCamera.get());

    // 3. Boss Tracking Window
    m_bossWindow = WindowManager::Instance().CreateGameWindow(
        "Boss Monitor",
        m_bossWindowSize[0],
        m_bossWindowSize[1]
    );
    m_bossCamera = std::make_shared<Camera>();
    m_bossWindow->SetCamera(m_bossCamera.get());
    m_bossWindow->SetPriority(0);

    m_cpuWindow = WindowManager::Instance().CreateGameWindow(
        "System Unit",
        m_cpuWindowSize[0],
        m_cpuWindowSize[1]
    );
    m_cpuCamera = std::make_shared<Camera>();
    m_cpuWindow->SetCamera(m_cpuCamera.get());

    m_monitor2Window = WindowManager::Instance().CreateGameWindow(
        "Side Monitor",
        m_monitor2WindowSize[0],
        m_monitor2WindowSize[1]
    );
    // Priority rendah agar di bawah monitor utama
    m_monitor2Window->SetPriority(4);

    m_monitor2Camera = std::make_shared<Camera>();
    m_monitor2Window->SetCamera(m_monitor2Camera.get());

    // Center Initial Positions
    auto CenterWindow = [&](GameWindow* win, const XMFLOAT3& pos, WindowState& winState) {
        float screenX, screenY;
        WorldToScreenPos(pos, screenX, screenY);

        winState.targetX = screenX - (win->GetWidth() * 0.5f);
        winState.targetY = screenY - (win->GetHeight() * 0.5f);

        // Round immediately untuk posisi awal
        winState.actualX = static_cast<int>(roundf(winState.targetX));
        winState.actualY = static_cast<int>(roundf(winState.targetY));

        SDL_SetWindowPosition(win->GetSDLWindow(), winState.actualX, winState.actualY);
        };

    if (m_player) CenterWindow(m_trackingWindow, m_player->GetPosition(), m_playerWindowState);
    //if (m_boss) CenterWindow(m_bossWindow, m_boss->GetPosition(), m_bossWindowState);

    //if (auto mainWin = Framework::Instance()->GetMainWindow()) mainWin->SetVisible(false);

    WindowManager::Instance().EnforceWindowPriorities();
    m_isWindowsInitialized = true;
}

// =========================================================
// SCREEN MATH UTILITIES (DRY Principle)
// =========================================================
void SceneGameBeyond::GetScreenDimensions(int& outWidth, int& outHeight) const
{
    // [BARU] Return cache jika masih valid
    if (m_cachedScreenWidth > 0 && m_cachedScreenHeight > 0)
    {
        outWidth = m_cachedScreenWidth;
        outHeight = m_cachedScreenHeight;
        return;
    }

    // [FALLBACK] Query OS jika cache kosong
    outWidth = GetSystemMetrics(SM_CXSCREEN);
    outHeight = GetSystemMetrics(SM_CYSCREEN);
    m_cachedScreenWidth = outWidth;
    m_cachedScreenHeight = outHeight;
}

void SceneGameBeyond::WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY) const
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

    // Center of screen is World (0,0), Z axis is inverted visually
    outScreenX = (screenW * 0.5f) + (worldPos.x * PIXEL_TO_UNIT_RATIO);
    outScreenY = (screenH * 0.5f) - (worldPos.z * PIXEL_TO_UNIT_RATIO);
}

float SceneGameBeyond::GetUnifiedCameraHeight() const
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) * 0.5f);
    return (screenH * 0.5f) / (PIXEL_TO_UNIT_RATIO * halfFovTan);
}

// =========================================================
// CORE UPDATE LOGIC
// =========================================================
void SceneGameBeyond::UpdateWindowTracking(float dt, GameWindow* win, Camera* cam,
    const DirectX::XMFLOAT3& targetPos, WindowState& winState)
{
    if (!win || !cam) return;

    // [OPTIMIZATION] Cache SDL window pointer
    SDL_Window* sdlWin = win->GetSDLWindow();
    if (!sdlWin) return;

    float targetScreenX, targetScreenY;
    WorldToScreenPos(targetPos, targetScreenX, targetScreenY);

    float destX = targetScreenX - (win->GetWidth() * 0.5f);
    float destY = targetScreenY - (win->GetHeight() * 0.5f);

    float t = min(m_windowFollowSpeed * dt, 1.0f);
    winState.targetX += (destX - winState.targetX) * t;
    winState.targetY += (destY - winState.targetY) * t;

    int newX = static_cast<int>(roundf(winState.targetX));
    int newY = static_cast<int>(roundf(winState.targetY));

    static constexpr int MIN_MOVEMENT_THRESHOLD = 2;

    int deltaX = abs(newX - winState.actualX);
    int deltaY = abs(newY - winState.actualY);

    // [OPTIMIZATION] Single SDL call for position update
    if (deltaX >= MIN_MOVEMENT_THRESHOLD || deltaY >= MIN_MOVEMENT_THRESHOLD)
    {
        // [BARU] Try-catch untuk handle SDL errors
        try {
            SDL_SetWindowPosition(sdlWin, newX, newY);
            winState.actualX = newX;
            winState.actualY = newY;
        }
        catch (...) {
            // Failsafe: mark window as invalid
            return;
        }
    }

    UpdateOffCenterProjection(cam, win, GetUnifiedCameraHeight());
}

void SceneGameBeyond::Update(float elapsedTime)
{
    // Update screen cache
    m_cacheUpdateTimer += elapsedTime;
    if (m_cacheUpdateTimer >= CACHE_REFRESH_INTERVAL)
    {
        m_cachedScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        m_cachedScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        m_cacheUpdateTimer = 0.0f;
    }

    // 1. Logika Menunggu Enter
    if (!m_gameStarted)
    {
        if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
        {
            GameWindow* mainWin = Framework::Instance()->GetMainWindow();
            if (mainWin)
            {
                if (m_player)
                {
                    auto pPos = m_player->GetPosition();
                    WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 12);
                }
                WindowManager::Instance().DestroyWindow(mainWin);
            }
            m_gameStarted = true;
            m_shatterTriggered = true;
        }
    }

    // 2. SINGLE DEFERRED WINDOW INIT CHECK
    if (!m_isWindowsInitialized)
    {
        m_startupTimer += elapsedTime;
        if (m_startupTimer > DEFERRED_INIT_TIME)
        {
            InitializeSubWindows();
        }
        return; // Exit early jika windows belum ready
    }

    // --- HAPUS DUPLICATE CHECK DI SINI! (Line 279-285) ---

    // 3. Update Managers
    WindowShatterManager::Instance().Update(elapsedTime);

    // Update Shatter Projections
    float unifiedHeight = GetUnifiedCameraHeight();
    for (const auto& shatter : WindowShatterManager::Instance().GetShatters())
    {
        if (shatter->IsNativeWindow())
        {
            UpdateOffCenterProjection(shatter->GetCamera(), shatter->GetWindow(), unifiedHeight);
        }
    }

    // 4. GAME LOGIC
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    if (m_player)
    {
        m_player->Update(elapsedTime, activeCam);
        if (m_subCamera) m_subCamera->LookAt(m_player->GetPosition());
    }

    if (m_boss) m_boss->Update(elapsedTime);

    if (m_blockManager)
    {
        m_blockManager->Update(elapsedTime, activeCam, m_player.get());
        if (Input::Instance().GetKeyboard().IsTriggered('R'))
            m_blockManager->SpawnAllyBlock(m_player.get());
    }

    // 5. WINDOW TRACKING
    if (m_player && m_trackingWindow && m_trackingCamera)
    {
        UpdateWindowTracking(elapsedTime, m_trackingWindow, m_trackingCamera.get(),
            m_player->GetPosition(), m_playerWindowState);
    }

    if (m_boss && m_bossWindow && m_bossCamera)
    {
        DirectX::XMFLOAT3 bossPos = m_boss->GetMonitorVisualPos();
        DirectX::XMFLOAT3 targetPos = {
            bossPos.x + m_bossTrackingOffset.x,
            0.0f,
            bossPos.z + m_bossTrackingOffset.z
        };
        UpdateWindowTracking(elapsedTime, m_bossWindow, m_bossCamera.get(),
            targetPos, m_bossWindowState);
    }

    if (m_boss && m_cpuWindow && m_cpuCamera)
    {
        DirectX::XMFLOAT3 cpuPos = m_boss->GetCPUVisualPos();
        DirectX::XMFLOAT3 targetPos = {
            cpuPos.x + m_cpuTrackingOffset.x,
            0.0f,
            cpuPos.z + m_cpuTrackingOffset.z
        };
        UpdateWindowTracking(elapsedTime, m_cpuWindow, m_cpuCamera.get(),
            targetPos, m_cpuWindowState);
    }

    if (m_boss && m_monitor2Window && m_monitor2Camera)
    {
        DirectX::XMFLOAT3 pos = m_boss->GetMonitor2VisualPos();
        DirectX::XMFLOAT3 targetPos = {
            pos.x + m_monitor2TrackingOffset.x,
            0.0f,
            pos.z + m_monitor2TrackingOffset.z
        };
        UpdateWindowTracking(elapsedTime, m_monitor2Window, m_monitor2Camera.get(),
            targetPos, m_monitor2WindowState);
    }

    UpdateLensLogic();

    m_priorityEnforceTimer += elapsedTime;
    if (m_priorityEnforceTimer >= PRIORITY_ENFORCE_INTERVAL)
    {
        WindowManager::Instance().EnforceWindowPriorities();
        m_priorityEnforceTimer = 0.0f;
    }

    // [PINDAH] EnforceWindowPriorities hanya dipanggil setelah init
    // atau saat ada perubahan priority (jangan tiap frame!)
    // WindowManager::Instance().EnforceWindowPriorities();

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}


// =========================================================
// RENDER & GUI
// =========================================================
// File: SceneGameBeyond.cpp

void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    // =========================================================
    // [BARU] BACKGROUND HITAM (Hanya jika belum start & Main Camera)
    // =========================================================
    if (!m_gameStarted && targetCam == m_mainCamera.get())
    {
        // Ambil ukuran layar
        D3D11_VIEWPORT vp;
        UINT num = 1;
        dc->RSGetViewports(&num, &vp);

        // Gambar Kotak Hitam Fullscreen
        m_primitive2D->Rect(0.0f, 0.0f, vp.Width, vp.Height, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        // Render SEKARANG JUGA agar tertulis di layar paling belakang
        m_primitive2D->Render(dc);

        // Reset Depth State agar Player 3D bisa digambar di atas warna hitam
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    }

    // 2. Render Scene 3D (Isinya sudah difilter, lihat fungsi RenderScene di bawah)
    RenderScene(elapsedTime, targetCam);

    if (targetCam == m_mainCamera.get())
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

    // =========================================================
    // 1. Render Grid (SEMBUNYIKAN SAAT INTRO)
    // =========================================================
    if (m_gameStarted) // [UBAH DI SINI] Hanya gambar grid kalau game sudah mulai
    {
        primRenderer->DrawGrid(20, 1);
        primRenderer->Render(dc, camera->GetView(), camera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    // =========================================================
    // 2. Render Models
    // =========================================================

    // PLAYER: Selalu digambar (Intro maupun Game)
    if (m_player) m_player->Render(modelRenderer);

    // BOSS MODEL: SEMBUNYIKAN SAAT INTRO (Hanya Text yang mau ditampilkan)
    if (m_boss && m_gameStarted) // [UBAH DI SINI]
    {
        m_boss->Render(modelRenderer);
    }

    // BLOCKS: SEMBUNYIKAN SAAT INTRO
    if (m_blockManager && m_gameStarted) // [UBAH DI SINI]
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (block->IsActive())
                block->Render(modelRenderer, m_blockManager->globalBlockColor);
        }
    }

    // Flush Model Renderer
    modelRenderer->Render(rc);

    // 3. [FIX] Render Virtual Shatters (Overlay 2D)
    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    const auto& shatters = WindowShatterManager::Instance().GetShatters();
    for (const auto& shatter : shatters)
    {
        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            float screenX, screenY;
            WorldToScreenPos(worldPos, screenX, screenY);

            m_primitive2D->Rect(
                screenX, screenY,
                size.x, size.y,
                size.x * 0.5f, size.y * 0.5f,
                0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            );
        }
    }

    // [FIX] Buat Matriks Orthographic untuk Screen Space Rendering
    // Ini memetakan koordinat Pixel (0..W, 0..H) ke koordinat GPU (-1..1)
    int sw, sh;
    GetScreenDimensions(sw, sh);
    DirectX::XMMATRIX viewOrtho = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX projOrtho = DirectX::XMMatrixOrthographicOffCenterLH(
        0.0f, (float)sw,  // Left, Right
        (float)sh, 0.0f,  // Bottom, Top (Y-Axis ke bawah)
        0.0f, 1.0f        // Near, Far
    );

    // Panggil Render dengan Matriks Ortho

    // Restore Depth State
    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::TestAndWrite), 0);

    // Render Boss Text
    if (m_boss) 
    {
        DirectX::XMFLOAT3 basePos = m_boss->GetMonitorVisualPos();
        DirectX::XMFLOAT3 finalPos = { basePos.x + m_textConfig.offset.x, basePos.y + m_textConfig.offset.y, basePos.z + m_textConfig.offset.z };
        DirectX::XMFLOAT3 radRotation = { XMConvertToRadians(m_textConfig.rotation.x), XMConvertToRadians(m_textConfig.rotation.y), XMConvertToRadians(m_textConfig.rotation.z) };

        auto font = ResourceManager::Instance().GetFont("VGA_FONT");
        font->Draw3D(m_textConfig.label, camera, finalPos, m_textConfig.scale, radRotation, m_textConfig.color);
    }
}

void SceneGameBeyond::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();

    ImGui::Begin("Scene Info");
    ImGui::Text("Controls: WASD (Move), N (New Window)");

    if (m_player) m_player->DrawDebugGUI();
    ImGui::Separator();
    if (m_boss) m_boss->DrawDebugGUI();

    if (ImGui::CollapsingHeader("Window Tracking Config"))
    {
        ImGui::DragFloat("Follow Speed", &m_windowFollowSpeed, 0.1f, 0.1f, 50.0f);

        ImGui::DragFloat3("Boss Monitor Offset", &m_bossTrackingOffset.x, 0.1f);
        
        ImGui::DragFloat3("CPU Monitor Offset", &m_cpuTrackingOffset.x, 0.1f);

        ImGui::DragFloat3("2nd Monitor Offset", &m_monitor2TrackingOffset.x, 0.1f);

        // 1. MONITOR SIZE
        if (ImGui::DragInt2("Monitor Size (WxH)", m_bossWindowSize, 1, 100, 1000))
        {
            if (m_bossWindow)
            {
                // [FIX] Perintah OS untuk ubah ukuran fisik window
                SDL_SetWindowSize(m_bossWindow->GetSDLWindow(), m_bossWindowSize[0], m_bossWindowSize[1]);

                // Update internal buffer engine (agar tidak stretch)
                m_bossWindow->Resize(m_bossWindowSize[0], m_bossWindowSize[1]);
            }
        }

        // 2. CPU SIZE
        if (ImGui::DragInt2("CPU Size (WxH)", m_cpuWindowSize, 1, 100, 1000))
        {
            if (m_cpuWindow)
            {
                // [FIX] Perintah OS untuk ubah ukuran fisik window
                SDL_SetWindowSize(m_cpuWindow->GetSDLWindow(), m_cpuWindowSize[0], m_cpuWindowSize[1]);

                // Update internal buffer engine
                m_cpuWindow->Resize(m_cpuWindowSize[0], m_cpuWindowSize[1]);
            }
        }

        if (ImGui::DragInt2("Side Monitor Size (WxH)", m_monitor2WindowSize, 1, 100, 1000))
        {
            if (m_monitor2Window)
            {
                SDL_SetWindowSize(m_monitor2Window->GetSDLWindow(), m_monitor2WindowSize[0], m_monitor2WindowSize[1]);
                m_monitor2Window->Resize(m_monitor2WindowSize[0], m_monitor2WindowSize[1]);
            }
        }

    }

    if (ImGui::CollapsingHeader("3D Text Editor"))
    {
        ImGui::InputText("Label", m_textConfig.label, sizeof(m_textConfig.label));
        ImGui::DragFloat3("Offset", &m_textConfig.offset.x, 0.1f);
        ImGui::DragFloat3("Rotation", &m_textConfig.rotation.x, 1.0f, -360.0f, 360.0f);
        ImGui::DragFloat("Scale", &m_textConfig.scale, 0.001f, 0.001f, 1.0f, "%.4f");
        ImGui::ColorEdit4("Color", &m_textConfig.color.x);

        if (ImGui::Button("Reset Text"))
        {
            m_textConfig = Text3DConfig(); // Reset to default struct values
        }
    }

    // --- PERBAIKAN: RENDER SHATTER OVERLAY MENGGUNAKAN IMGUI ---
    // Gunakan ForegroundDrawList agar digambar paling atas (seperti HUD)
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const auto& shatters = WindowShatterManager::Instance().GetShatters();

    for (const auto& shatter : shatters)
    {
        if (!shatter->IsNativeWindow()) // Jika masih berupa kotak virtual
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            // Konversi posisi World ke Screen (Pixel)
            float screenX, screenY;
            WorldToScreenPos(worldPos, screenX, screenY);

            // Gambar kotak putih solid
            // (Posisi X,Y dikurangi setengah ukuran agar pivot di tengah)
            drawList->AddRectFilled(
                ImVec2(screenX - size.x * 0.5f, screenY - size.y * 0.5f), // Kiri-Atas
                ImVec2(screenX + size.x * 0.5f, screenY + size.y * 0.5f), // Kanan-Bawah
                IM_COL32(255, 255, 255, 255) // Warna Putih
            );
        }
    }

    if (ImGui::CollapsingHeader("Window Shatter Debug"))
    {
        ImGui::Text("Active Shatter: %d", WindowShatterManager::Instance().GetActiveCount());

        if (ImGui::Button("Spawn Test Explosion"))
        {
            WindowShatterManager::Instance().TriggerExplosion(DirectX::XMFLOAT2(0.0f, 0.0f), 10);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear All")) WindowShatterManager::Instance().Clear();

        if (ImGui::Button("Reset Intro"))
        {
            m_shatterTriggered = false;
            m_introTimer = 0.0f;
        }
    }
    ImGui::End();
}

void SceneGameBeyond::UpdateLensLogic()
{
    if (!m_lensWindow || !m_lensCamera) return;
    UpdateOffCenterProjection(m_lensCamera.get(), m_lensWindow, GetUnifiedCameraHeight());
}

void SceneGameBeyond::UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight)
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

    targetCam->SetPosition(0.0f, camHeight, 0.0f);
    targetCam->LookAt({ 0.0f, 0.0f, 0.0f });

    // CRITICAL: Get ACTUAL window position from SDL (sudah stable)
    int winX, winY;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);

    // CRITICAL: Get ACTUAL window size (bisa berubah karena resize)
    int winW, winH;
    SDL_GetWindowSize(targetWin->GetSDLWindow(), &winW, &winH);

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) * 0.5f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    // Convert pixel to projection space [-1, 1]
    // Use DOUBLE precision for calculation to reduce error
    double screenWd = static_cast<double>(screenW);
    double screenHd = static_cast<double>(screenH);
    double winXd = static_cast<double>(winX);
    double winYd = static_cast<double>(winY);
    double winWd = static_cast<double>(winW);
    double winHd = static_cast<double>(winH);

    float l = static_cast<float>((winXd / screenWd) * 2.0 - 1.0);
    float r = static_cast<float>(((winXd + winWd) / screenWd) * 2.0 - 1.0);
    float t = static_cast<float>(1.0 - (winYd / screenHd) * 2.0);
    float b = static_cast<float>(1.0 - ((winYd + winHd) / screenHd) * 2.0);

    targetCam->SetOffCenterProjection(
        l * halfWidth,
        r * halfWidth,
        b * halfHeight,
        t * halfHeight,
        nearZ,
        farZ
    );
}

void SceneGameBeyond::OnResize(int width, int height)
{
    if (m_mainCamera) m_mainCamera->SetAspectRatio((float)width / max(1, height));
}

void SceneGameBeyond::HandleDebugInput()
{
    if (Input::Instance().GetKeyboard().IsTriggered('N'))
    {
        GameWindow* addWin = WindowManager::Instance().CreateGameWindow("Debug Cam", 300, 300);
        auto addCam = std::make_shared<Camera>();
        addCam->SetPerspectiveFov(XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
        addWin->SetCamera(addCam.get());
        m_additionalCameras.push_back(addCam);
    }
}