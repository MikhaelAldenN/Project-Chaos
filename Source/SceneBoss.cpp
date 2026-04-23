#include "SceneBoss.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "WindowManager.h"
#include "Framework.h"
#include <algorithm> // For std::clamp
#include <imgui.h>

using namespace DirectX;

// =========================================================
// LOCAL CONSTANTS
// =========================================================
constexpr float FIELD_OF_VIEW = 60.0f;
constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;

DirectX::XMFLOAT3 GetMouseWorldPosWindowkill()
{
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    float monitorCenterX = GetSystemMetrics(SM_CXSCREEN) / 2.0f;
    float monitorCenterY = GetSystemMetrics(SM_CYSCREEN) / 2.0f;

    float pixelOffsetX = static_cast<float>(cursorPos.x) - monitorCenterX;
    float pixelOffsetY = static_cast<float>(cursorPos.y) - monitorCenterY;

    float worldX = pixelOffsetX / PIXEL_TO_UNIT_RATIO;
    float worldZ = -pixelOffsetY / PIXEL_TO_UNIT_RATIO;

    return DirectX::XMFLOAT3(worldX, 0.0f, worldZ);
}

// =========================================================
// CONSTRUCTOR
// =========================================================
SceneBoss::SceneBoss()
{
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    float screenW = 1920.0f;
    float screenH = 1080.0f;

    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(PIXEL_TO_UNIT_RATIO);
    m_windowSystem->SetFOV(FIELD_OF_VIEW);

    float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();

    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    m_player = std::make_unique<Player>();
    m_player->SetInvertControls(false);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);

    auto device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
    m_primitive3D = std::make_unique<PrimitiveRenderer>(device);

    InitializeSubWindows();
    AddLog("SceneBoss Initialized. Windowkill System Online.");
}

SceneBoss::~SceneBoss()
{
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    CameraController::Instance().ClearCamera();
}

// =========================================================
// FITUR DEBUG HELPER
// =========================================================
void SceneBoss::AddLog(const std::string& message)
{
    m_debugLogs.push_back(message);
    if (m_debugLogs.size() > 50) m_debugLogs.erase(m_debugLogs.begin()); // Limit 50 baris
}

void SceneBoss::ResetEverything()
{
    m_windowSystem->ClearAll();
    InitializeSubWindows();

    if (m_player) m_player->SetPosition(0.0f, 0.0f, -8.0f);

    m_timeScale = 1.0f;
    m_spawnCount = 0;
    AddLog("SYSTEM RESET: All windows closed and re-initialized.");
}

void SceneBoss::SpawnDebugWindow()
{
    m_spawnCount++;
    TrackedWindowConfig config;
    config.name = "debug_win_" + std::to_string(m_spawnCount);
    config.title = "D" + std::to_string(m_spawnCount) + " (drag/stretch me!)";
    config.width = 300;
    config.height = 300;
    config.role = WindowRole::SUB_VIEWPORT;

    // AddTrackedWindow otomatis akan membuatkan Kamera independen yang tertaut sempurna!
    m_windowSystem->AddTrackedWindow(config, []() {
        return DirectX::XMFLOAT3(0, 0, 0);
        });

    // Modifikasi sifat OS Window-nya saja
    for (auto& tracked : m_windowSystem->GetWindows())
    {
        if (tracked->name == config.name)
        {
            SDL_Window* sdlWin = tracked->window->GetSDLWindow();
            SDL_SetWindowResizable(sdlWin, true);
            SDL_SetWindowBordered(sdlWin, true);
            break;
        }
    }

    AddLog("Spawned stable portal: " + config.name);
}

// =========================================================
// INITIALIZATION
// =========================================================
void SceneBoss::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player) return;

    m_windowSystem->AddTrackedWindow(
        { "player", "Player", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },
        [this]() -> DirectX::XMFLOAT3 {
            if (!m_player) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
            auto pPos = m_player->GetPosition();
            return DirectX::XMFLOAT3(pPos.x, 0.0f, pPos.z);
        },
        [this]() -> DirectX::XMFLOAT2 {
            float safeMarginPixel = 150.0f;
            float extraPadding = 0.0f;
            float size = (safeMarginPixel * 2.0f) + extraPadding;
            return DirectX::XMFLOAT2(size, size);
        }
    );

    TrackedWindow* playerWin = m_windowSystem->GetTrackedWindow("player");
    if (playerWin && playerWin->window)
    {
        SDL_Window* sdlWin = playerWin->window->GetSDLWindow();
        SDL_SetWindowResizable(sdlWin, true);
        SDL_SetWindowBordered(sdlWin, true);

        // Opsional: Kalau kamu mau player window juga bisa digeser manual pakai mouse di titlebarnya
        // aktifkan baris di bawah ini untuk menimpa aturan hardcoded di AddTrackedWindow.
        // playerWin->window->SetDraggable(true); 
    }

    GameWindow* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow())
    {
        SDL_ShowWindow(mainWindow->GetSDLWindow());

        // [FIX IMGUI] Cek dulu! Kalau belum terdaftar, baru register.
        if (!m_windowSystem->GetTrackedWindow("main_window"))
        {
            m_windowSystem->RegisterWindow(mainWindow, WindowRole::MAIN_VIEWPORT, m_mainCamera);
        }
    }
    WindowManager::Instance().EnforceWindowPriorities();
}

// =========================================================
// CORE UPDATE
// =========================================================
void SceneBoss::Update(float elapsedTime)
{
    // Aplikasi Time Scale (Slow Motion)
    float scaledDt = elapsedTime * m_timeScale;

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // 1. SINKRONISASI MAIN WINDOW KE PANEL IMGUI
    GameWindow* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow)
    {
        if (m_autoSyncMainWindow)
        {
            GameWindow* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
            if (mainWindow)
            {
                // HAPUS baris SDL_SetWindowPosition agar kita bisa drag manual
                //SDL_SetWindowPosition(mainWindow->GetSDLWindow(), 0, 0); 

                // Ambil posisi window saat ini (hasil drag user)
                int winX, winY;
                SDL_GetWindowPosition(mainWindow->GetSDLWindow(), &winX, &winY);

                // Update bounds agar ImGui tetap di dalam window tersebut
                SDL_SetWindowSize(mainWindow->GetSDLWindow(), (int)m_debugPanelSize.x, (int)m_debugPanelSize.y);
                m_windowSystem->UpdateWindowBounds(0, (int)m_debugPanelSize.x, (int)m_debugPanelSize.y);
            }
        }
        else
        {
            int currentW = mainWindow->GetWidth();
            int currentH = mainWindow->GetHeight();
            m_windowSystem->UpdateWindowBounds(0, currentW, currentH);
        }
    }

    // 2. Update Player
    if (m_player)
    {
        m_player->Update(scaledDt, activeCam);

        DirectX::XMFLOAT3 pos = m_player->GetPosition();
        pos.x = std::clamp(pos.x, -m_screenLimitX, m_screenLimitX);
        pos.z = std::clamp(pos.z, -m_screenLimitZ, m_screenLimitZ);
        pos.y = max(pos.y, 0.0f);
        m_player->SetPosition(pos.x, pos.y, pos.z);

        if (m_mainCamera)
        {
            DirectX::XMFLOAT3 targetPos = GetMouseWorldPosWindowkill();
            m_player->RotateModelToPoint(targetPos);
        }
    }

    // 3. Update Systems
    if (m_windowSystem)
    {
        for (auto& tracked : m_windowSystem->GetWindows())
        {
            // Jika window punya kamera dan bukan kamera utama
            if (tracked->camera && tracked->camera != m_mainCamera)
            {
                // Contek posisi dan rotasi dari Main Camera
                tracked->camera->SetPosition(m_mainCamera->GetPosition());
                tracked->camera->SetRotation(m_mainCamera->GetRotation());
                tracked->camera->LookAt(m_player->GetPosition()); // Atau titik pusat (0,0,0)
            }
        }
    }

    if (m_windowSystem)
    {
        m_windowSystem->Update(elapsedTime);

        // --- [TAMBAHKAN BLOK SINKRONISASI INI] ---
        for (auto& tracked : m_windowSystem->GetWindows())
        {
            // Jika ia adalah Portal tambahan (Dummy Window)
            if (tracked->role == WindowRole::SUB_VIEWPORT && tracked->camera)
            {
                // Contek posisi dan rotasi persis dari Kamera Utama
                tracked->camera->SetPosition(m_mainCamera->GetPosition());
                tracked->camera->SetRotation(m_mainCamera->GetRotation());
            }
        }
        // ----------------------------------------
    }

    CameraController::Instance().Update(scaledDt);
}

// =========================================================
// CORE RENDER
// =========================================================
void SceneBoss::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // TOGGLE GRID: Hanya gambar grid jika dicentang di GUI
    if (m_showGrid && m_primitive3D)
    {
        m_primitive3D->DrawGrid(25, 1.0f);
        m_primitive3D->Render(dc, targetCam->GetView(), targetCam->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
}

void SceneBoss::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    if (m_player)
    {
        XMFLOAT3 pPos = m_player->GetPosition();
        if (camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f))
        {
            m_player->Render(modelRenderer);
        }
    }

    modelRenderer->Render(rc);
}

// =========================================================
// DEBUG GUI
// =========================================================
void SceneBoss::DrawGUI()
{
    // Kunci posisi GUI selalu di pojok kiri atas layar!
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(m_debugPanelSize, ImGuiCond_FirstUseEver);

    // Flag NoMove agar panel tidak bisa digeser-geser sembarangan
    ImGui::Begin("WINDOWKILL MASTER CONTROL", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    m_debugPanelSize = ImGui::GetWindowSize();

    // ---------------------------------------------------------
    // TAB 1: SYSTEM METRICS
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("System Metrics & Time", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float fps = ImGui::GetIO().Framerate;
        ImVec4 fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Default: Hijau

        if (fps < 40.0f) {
            fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Merah
        }
        else if (fps < 50.0f) {
            fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Kuning
        }
        ImGui::TextColored(fpsColor , "FPS: %.1f (%.2f ms) (capped on 60 fps)", fps, 1000.0f / fps);

        static float values[90] = { 0 };
        static int values_offset = 0;
        values[values_offset] = 1000.0f / fps;
        values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
        ImGui::PlotLines("Frametime", values, IM_ARRAYSIZE(values), values_offset, nullptr, 0.0f, 33.0f, ImVec2(0, 50));

        // TIME SCALE (SLOW MO)
        ImGui::Separator();
        ImGui::SliderFloat("Time Scale", &m_timeScale, 0.1f, 3.0f, "%.1fx");
        if (ImGui::Button("Reset Time (1.0x)")) m_timeScale = 1.0f;
    }

    // ---------------------------------------------------------
    // TAB 2: WINDOW MANAGEMENT
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("Window Tracking Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Sync Main Window to This Panel", &m_autoSyncMainWindow);

        size_t activeCount = m_windowSystem->GetWindows().size();
        ImGui::Text("Active Windows: %zu", activeCount);

        if (ImGui::Button("Spawn Dummy Window", ImVec2(-1, 30))) {
            SpawnDebugWindow();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Close All Dummy Window", ImVec2(-1, 30))) {
            m_windowSystem->ClearAll();
            InitializeSubWindows();
            m_spawnCount = 0;
            AddLog("Cleared and respawned base windows.");
        }

        if (ImGui::Button("RESET", ImVec2(-1, 40))) {
            ResetEverything();
        }
        ImGui::PopStyleColor();
    }

    // ---------------------------------------------------------
    // TAB 3: PLAYER & WORLD
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("World & Entities", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show 3D Grid", &m_showGrid);

        if (m_player) {
            DirectX::XMFLOAT3 pPos = m_player->GetPosition();
            float rotY = m_player->GetMovement()->GetRotation().y;
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[Player]");
            ImGui::Text("Loc: X:%.2f, Z:%.2f", pPos.x, pPos.z);
            ImGui::Text("Facing Angle: %.1f deg", rotY);
        }

        ImGui::Separator();
        POINT mPos; GetCursorPos(&mPos);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "[Mouse]");
        ImGui::Text("Monitor OS: X:%d, Y:%d", mPos.x, mPos.y);
    }

    // ---------------------------------------------------------
    // TAB 4: LIVE TERMINAL LOG
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("Debug Terminal", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginChild("LogRegion", ImVec2(0, 150), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (const auto& log : m_debugLogs)
        {
            ImGui::TextUnformatted(log.c_str());
        }
        // Auto-scroll ke bawah jika ada log baru
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    ImGui::End();
}

void SceneBoss::OnResize(int width, int height)
{
    // Kosongkan agar tidak merusak kalkulasi Off-Center Projection
}

void SceneBoss::CloseSubWindowBySDLID(Uint32 sdlWindowID)
{
    if (!m_windowSystem) return;

    std::string targetName = "";

    // Cari nama window berdasarkan SDL Window ID
    for (const auto& tracked : m_windowSystem->GetWindows())
    {
        if (tracked->window && SDL_GetWindowID(tracked->window->GetSDLWindow()) == sdlWindowID)
        {
            targetName = tracked->name;
            break;
        }
    }

    if (!targetName.empty())
    {
        // Hancurkan windownya dan bersihkan kameranya!
        m_windowSystem->RemoveTrackedWindow(targetName);
        AddLog("Dummy Window Closed Manually: " + targetName);
    }
}
