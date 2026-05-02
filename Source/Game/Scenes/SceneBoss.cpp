#include "SceneBoss.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "WindowManager.h"
#include "Framework.h"
#include "InputHelper.h"
#include "PerformanceLogger.h"
#include <algorithm>
#include <imgui.h>

using namespace DirectX;

// =========================================================
// CONSTRUCTOR / DESTRUCTOR
// =========================================================

SceneBoss::SceneBoss()
{
    PerformanceLogger::Instance().Initialize();
    PerformanceLogger::Instance().LogInfo("[INIT] SceneBoss constructor begin.");

    // Disable ImGui multi-viewport while in this scene (restored in destructor)
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    // --- Window Tracking System ---
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(k_pixelToUnitRatio);
    m_windowSystem->SetFOV(k_fov);

    // --- Camera ---
    // Use a fixed 16:9 aspect for the projection; off-center projection per
    // sub-window is handled by WindowTrackingSystem::UpdateOffCenterProjection.
    const float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(k_fov), 1920.0f / 1080.0f, k_camNear, k_camFar);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // --- PhysX (minimal: no ground plane, gravity = zero) ---
    InitializePhysics();

    // --- Player ---
    m_player = std::make_unique<Player>();
    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get(),
        PlayerConst::CapsuleHalfHeight);  // Kaki tepat di Y=0, gravity off
    m_player->SetGravityEnabled(false);
    m_player->SetInvertControls(false);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);
    m_player->SetMoveSpeed(20.0f);

    // --- Primitive Renderers ---
    ID3D11Device* device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
    m_primitive3D = std::make_unique<PrimitiveRenderer>(device);

    WindowManager::Instance().SetTopmost(m_topmostEnabled);
    InitializeSubWindows();

    AddLog("SceneBoss initialized. Windowkill system online.");
    PerformanceLogger::Instance().LogInfo("[INIT] SceneBoss constructor complete.");
}

SceneBoss::~SceneBoss()
{
    // Restore ImGui multi-viewport for other scenes
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    CameraController::Instance().ClearCamera();

    // Player must be destroyed before PhysX (it holds a PxController raw ptr)
    m_player.reset();

    PerformanceLogger::Instance().Shutdown();
}

// =========================================================
// INITIALIZATION HELPERS
// =========================================================

void SceneBoss::InitializePhysics()
{
    m_foundation.reset(PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback));
    assert(m_foundation && "CRITICAL ERROR: PxCreateFoundation failed!");

    m_physics.reset(PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale(), true, nullptr));
    assert(m_physics && "CRITICAL ERROR: PxCreatePhysics failed!");

    // Zero gravity: player Y is clamped manually in Update; no ground plane needed.
    physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, 0.0f, 0.0f);
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

    m_dispatcher.reset(physx::PxDefaultCpuDispatcherCreate(2));
    sceneDesc.cpuDispatcher = m_dispatcher.get();

    m_scene.reset(m_physics->createScene(sceneDesc));
    assert(m_scene && "CRITICAL ERROR: createScene failed!");

    m_controllerManager.reset(PxCreateControllerManager(*m_scene));
    assert(m_controllerManager && "CRITICAL ERROR: PxCreateControllerManager failed!");

    m_defaultMaterial.reset(m_physics->createMaterial(0.5f, 0.5f, 0.1f));
    assert(m_defaultMaterial && "CRITICAL ERROR: createMaterial failed!");
}

void SceneBoss::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player) return;

    // --- Player-tracking window ---
    m_windowSystem->AddTrackedWindow(
        { "player", "Player", 300, 300, 1 },
        [this]() -> XMFLOAT3 {
            if (!m_player) return XMFLOAT3(0.0f, 0.0f, 0.0f);
            const auto pPos = m_player->GetPosition();
            return XMFLOAT3(
                pPos.x + m_stretchOffset.x,
                0.0f,
                pPos.z + m_stretchOffset.y   // XMFLOAT2::y maps to world Z
            );
        },
        [this]() -> XMFLOAT2 {
            return XMFLOAT2(
                k_defaultWinSize + m_currentStretch.x,
                k_defaultWinSize + m_currentStretch.y
            );
        }
    );

    TrackedWindow* playerWin = m_windowSystem->GetTrackedWindow("player");
    if (playerWin && playerWin->window)
    {
        SDL_Window* sdlWin = playerWin->window->GetSDLWindow();
        SDL_SetWindowResizable(sdlWin, true);
        SDL_SetWindowBordered(sdlWin, true);
    }

    // --- Register main window with the tracking system ---
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow())
    {
        SDL_ShowWindow(mainWindow->GetSDLWindow());

        if (!m_windowSystem->GetTrackedWindow("main_window"))
            m_windowSystem->RegisterWindow(mainWindow, WindowRole::MAIN_VIEWPORT, m_mainCamera);
    }

    WindowManager::Instance().EnforceWindowPriorities();
}

// =========================================================
// UPDATE
// =========================================================

void SceneBoss::Update(float elapsedTime)
{
    PerformanceLogger::Instance().StartTimer(PerfBucket::Logic);

    const float scaledDt = elapsedTime * m_timeScale;

    // --- PhysX tick ---
    if (m_scene)
    {
        m_scene->simulate(scaledDt);
        m_scene->fetchResults(true);
    }

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // --- Sync main window size ---
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow)
    {
        if (m_autoSyncMainWindow)
        {
            int winX, winY;
            SDL_GetWindowPosition(mainWindow->GetSDLWindow(), &winX, &winY);
            SDL_SetWindowSize(mainWindow->GetSDLWindow(),
                static_cast<int>(m_debugPanelSize.x),
                static_cast<int>(m_debugPanelSize.y));
            m_windowSystem->UpdateWindowBounds(0,
                static_cast<int>(m_debugPanelSize.x),
                static_cast<int>(m_debugPanelSize.y));
        }
        else
        {
            m_windowSystem->UpdateWindowBounds(0, mainWindow->GetWidth(), mainWindow->GetHeight());
        }
    }

    // --- Player update ---
// --- Player update ---
    if (m_player)
    {
        // Aim: convert global mouse to world position and pass to player
        const XMFLOAT3 mouseWorldPos =
            Beyond::InputHelper::GetMouseWorldPos(m_mainCamera->GetPosition());
        m_player->RotateModelToPoint(mouseWorldPos);

        m_player->Update(scaledDt, activeCam);

        // 1. Ambil resolusi layar laptop/monitor yang sebenarnya via OS
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // 2. Kalkulasi batas X dan Z dunia murni berdasarkan ukuran monitor.
        // Beri margin 1.0 unit (setara 40 pixel) sebagai ruang untuk badan model 3D,
        // persis seperti saat kamu menggunakan angka 23.0f (24 - 1) sebelumnya.
        float playerRadiusMargin = 1.0f;

        float limitX = ((screenWidth / 2.0f) / k_pixelToUnitRatio) - playerRadiusMargin;
        float limitZ = ((screenHeight / 2.0f) / k_pixelToUnitRatio) - playerRadiusMargin;

        // Pastikan limit tidak tembus minus jika monitor entah kenapa terbaca aneh
        limitX = max(0.0f, limitX);
        limitZ = max(0.0f, limitZ);

        // 3. Apply clamp ke posisi PLAYER
        XMFLOAT3 pos = m_player->GetPosition();
        pos.x = std::clamp(pos.x, -limitX, limitX);
        pos.z = std::clamp(pos.z, -limitZ, limitZ);
        pos.y = max(pos.y, 0.0f);

        m_player->SetPosition(pos);
    }
    // --- Squash & Stretch (elastic window deformation during dash) ---
    if (m_player)
    {
        const XMFLOAT3 vel = m_player->GetMovement()->GetVelocity();
        const float    currentSpeedSq = vel.x * vel.x + vel.z * vel.z;
        const float    dashThreshold = m_player->GetDashSpeed() * 0.8f;

        XMFLOAT2 targetStretch = { 0.0f, 0.0f };
        XMFLOAT2 targetOffset = { 0.0f, 0.0f };
        constexpr float lerpSpeed = 10.0f;

        if (currentSpeedSq > (dashThreshold * dashThreshold))
        {
            constexpr float kStretchX = 200.0f;
            constexpr float kSquashY = -40.0f;
            constexpr float kStretchZ = 120.0f;
            constexpr float kSquashX = -30.0f;

            const float dashRatio = sqrtf(currentSpeedSq) / m_player->GetDashSpeed();

            if (std::abs(vel.x) > std::abs(vel.z))
            {
                // Horizontal dash: stretch X, squash Y
                targetStretch.x = dashRatio * kStretchX;
                targetStretch.y = dashRatio * kSquashY;
                const float signX = (vel.x > 0.0f) ? 1.0f : -1.0f;
                targetOffset.x = -signX * (targetStretch.x * 0.5f) / k_pixelToUnitRatio;
            }
            else
            {
                // Vertical dash: stretch Z (Y pixel), squash X
                targetStretch.y = dashRatio * kStretchZ;
                targetStretch.x = dashRatio * kSquashX;
                const float signZ = (vel.z > 0.0f) ? 1.0f : -1.0f;
                targetOffset.y = -signZ * (targetStretch.y * 0.5f) / k_pixelToUnitRatio;
            }
        }

        m_currentStretch.x += (targetStretch.x - m_currentStretch.x) * lerpSpeed * scaledDt;
        m_currentStretch.y += (targetStretch.y - m_currentStretch.y) * lerpSpeed * scaledDt;
        m_stretchOffset.x += (targetOffset.x - m_stretchOffset.x) * lerpSpeed * scaledDt;
        m_stretchOffset.y += (targetOffset.y - m_stretchOffset.y) * lerpSpeed * scaledDt;
    }

    // --- Sync sub-window cameras to match main camera ---
    if (m_windowSystem)
    {
        for (auto& tracked : m_windowSystem->GetWindows())
        {
            if (!tracked->camera || tracked->camera == m_mainCamera) continue;

            tracked->camera->SetPosition(m_mainCamera->GetPosition());
            tracked->camera->SetRotation(m_mainCamera->GetRotation());

            // SUB_VIEWPORT portals just mirror the main camera orientation
            if (tracked->role != WindowRole::SUB_VIEWPORT && m_player)
                tracked->camera->LookAt(m_player->GetPosition());
        }

        m_windowSystem->Update(elapsedTime);
    }

    CameraController::Instance().Update(scaledDt);

    // --- Performance bookkeeping ---
    PerformanceLogger::Instance().StopTimer(PerfBucket::Logic);
    const int activeWins = m_windowSystem ? static_cast<int>(m_windowSystem->GetWindows().size()) : 0;
    PerformanceLogger::Instance().EndFrameCheck(ImGui::GetIO().Framerate, activeWins);
}

// =========================================================
// RENDER
// =========================================================

void SceneBoss::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // Detect whether this render call is targeting a transparent sub-window
    bool isTransparentWindow = false;
    if (m_windowSystem)
    {
        for (const auto& tracked : m_windowSystem->GetWindows())
        {
            if (tracked->camera.get() == camera &&
                tracked->window &&
                tracked->window->IsTransparent())
            {
                isTransparentWindow = true;
                break;
            }
        }
    }

    // Transparent windows need TransparentWindow blend state so the alpha
    // channel is preserved for UpdateLayeredWindow. Normal windows use
    // standard alpha blending.
    dc->OMSetBlendState(
        rs->GetBlendState(isTransparentWindow
            ? BlendState::TransparentWindow
            : BlendState::Transparency),
        nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam, isTransparentWindow);

    if (m_showGrid && m_primitive3D)
    {
        m_primitive3D->DrawGrid(25, 1.0f);
        m_primitive3D->Render(dc,
            targetCam->GetView(),
            targetCam->GetProjection(),
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    Graphics::Instance().GetShapeRenderer()->Render(
        dc, targetCam->GetView(), targetCam->GetProjection());
}

void SceneBoss::RenderScene(float elapsedTime, Camera* camera, bool isTransparentWindow)
{
    PerformanceLogger::Instance().StartTimer(PerfBucket::Render3D);

    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();

    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };
    rc.isTransparentWindow = isTransparentWindow;

    if (m_player)
    {
        const XMFLOAT3 pPos = m_player->GetPosition();
        if (camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f))
            m_player->Render(modelRenderer);
    }

    modelRenderer->Render(rc);

    PerformanceLogger::Instance().StopTimer(PerfBucket::Render3D);
}

// =========================================================
// GUI
// =========================================================

void SceneBoss::DrawGUI()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(m_debugPanelSize, ImGuiCond_FirstUseEver);

    ImGui::Begin("WINDOWKILL MASTER CONTROL", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    m_debugPanelSize = ImGui::GetWindowSize();

    // ---------------------------------------------------------
    // SYSTEM METRICS
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("System Metrics & Time", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float fps = ImGui::GetIO().Framerate;
        ImVec4 fpsColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (fps < 40.0f) fpsColor = { 1.0f, 0.0f, 0.0f, 1.0f };
        else if (fps < 50.0f) fpsColor = { 1.0f, 1.0f, 0.0f, 1.0f };

        ImGui::TextColored(fpsColor, "FPS: %.1f (%.2f ms) [cap: 60]", fps, 1000.0f / fps);

        static float s_frametimes[90] = {};
        static int   s_offset = 0;
        s_frametimes[s_offset] = 1000.0f / fps;
        s_offset = (s_offset + 1) % IM_ARRAYSIZE(s_frametimes);
        ImGui::PlotLines("Frametime", s_frametimes, IM_ARRAYSIZE(s_frametimes),
            s_offset, nullptr, 0.0f, 33.0f, ImVec2(0, 50));

        ImGui::Separator();
        ImGui::SliderFloat("Time Scale", &m_timeScale, 0.1f, 3.0f, "%.1fx");
        if (ImGui::Button("Reset Time (1.0x)")) m_timeScale = 1.0f;
    }

    // ---------------------------------------------------------
    // WINDOW MANAGEMENT
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("Window Tracking Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("[All] Toggle Topmost (triggers reset)", &m_topmostEnabled))
        {
            WindowManager::Instance().SetTopmost(m_topmostEnabled);
            ResetEverything();
        }

        if (ImGui::Checkbox("[Player] Toggle Transparent", &m_playerWindowTransparent))
        {
            m_windowSystem->RemoveTrackedWindow("player");

            TrackedWindowConfig config;
            config.name = "player";
            config.title = "Player";
            config.width = 300;
            config.height = 300;
            config.priority = 1;
            config.isTransparent = m_playerWindowTransparent;

            m_windowSystem->AddTrackedWindow(
                config,
                [this]() -> XMFLOAT3 {
                    if (!m_player) return XMFLOAT3(0.0f, 0.0f, 0.0f);
                    const auto pPos = m_player->GetPosition();
                    return XMFLOAT3(
                        pPos.x + m_stretchOffset.x,
                        0.0f,
                        pPos.z + m_stretchOffset.y
                    );
                },
                [this]() -> XMFLOAT2 {
                    return XMFLOAT2(
                        k_defaultWinSize + m_currentStretch.x,
                        k_defaultWinSize + m_currentStretch.y
                    );
                }
            );

            TrackedWindow* playerWin = m_windowSystem->GetTrackedWindow("player");
            if (playerWin && playerWin->window)
            {
                if (m_playerWindowTransparent)
                {
                    playerWin->window->SetClickThrough(true);
                    playerWin->window->SetBorderVisible(false);
                    playerWin->window->SetDraggable(false);
                    playerWin->window->SetBackgroundAlpha(0.0f);
                }
                else
                {
                    SDL_SetWindowResizable(playerWin->window->GetSDLWindow(), true);
                    SDL_SetWindowBordered(playerWin->window->GetSDLWindow(), true);
                    playerWin->window->SetDraggable(false);
                    playerWin->window->SetBackgroundAlpha(1.0f);
                }
            }

            WindowManager::Instance().EnforceWindowPriorities();
            AddLog(m_playerWindowTransparent
                ? "Player Window: Stealth Mode Activated"
                : "Player Window: Normal Mode");
        }

        ImGui::Checkbox("[ImGui] Sync size to main window", &m_autoSyncMainWindow);
        ImGui::Separator();

        ImGui::Text("Active Windows: %zu", m_windowSystem->GetWindows().size());

        if (ImGui::Button("Spawn Dummy Window", ImVec2(-1.0f, 30.0f)))
            SpawnDebugWindow();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.8f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

        if (ImGui::Button("Spawn Transparent Window (Hollow)", ImVec2(-1.0f, 30.0f)))
            SpawnTransparentWindow(0.0f, "Hollow");

        if (ImGui::Button("Spawn Transparent Window (Solid)", ImVec2(-1.0f, 30.0f)))
            SpawnTransparentWindow(1.0f / 255.0f, "Solid");

        ImGui::PopStyleColor(3);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

        if (ImGui::Button("Close All Sub Windows", ImVec2(-1.0f, 30.0f)))
        {
            m_windowSystem->ClearAll();
            InitializeSubWindows();
            m_spawnCount = 0;
            AddLog("Cleared and respawned base windows.");
        }

        if (ImGui::Button("HARD RESET", ImVec2(-1.0f, 40.0f)))
            ResetEverything();

        ImGui::PopStyleColor();
    }

    // ---------------------------------------------------------
    // WORLD & ENTITIES
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("World & Entities", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show 3D Grid", &m_showGrid);

        if (m_player)
        {
            const XMFLOAT3 pPos = m_player->GetPosition();
            const float    rotY = m_player->GetMovement()->GetRotation().y;
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[Player]");
            ImGui::Text("Loc: X:%.2f, Y:%.2f, Z:%.2f", pPos.x, pPos.y, pPos.z);
            ImGui::Text("Facing: %.1f deg", rotY);
        }

        ImGui::Separator();
        POINT mPos; GetCursorPos(&mPos);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "[Mouse]");
        ImGui::Text("Monitor OS: X:%d, Y:%d", mPos.x, mPos.y);
    }

    // ---------------------------------------------------------
    // DEBUG TERMINAL
    // ---------------------------------------------------------
    if (ImGui::CollapsingHeader("Debug Terminal", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginChild("LogRegion", ImVec2(0.0f, 150.0f), true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (const auto& log : m_debugLogs)
            ImGui::TextUnformatted(log.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    ImGui::End();
}

void SceneBoss::OnResize(int /*width*/, int /*height*/)
{
    // Intentionally empty: off-center projection is recalculated per sub-window
    // every frame inside WindowTrackingSystem::UpdateOffCenterProjection.
}

// =========================================================
// DEBUG / SYSTEM HELPERS
// =========================================================

void SceneBoss::AddLog(const std::string& message)
{
    m_debugLogs.push_back(message);
    if (m_debugLogs.size() > 50)
        m_debugLogs.erase(m_debugLogs.begin());
}

void SceneBoss::ResetEverything()
{
    // 1. Detach camera before destroying anything
    CameraController::Instance().ClearCamera();

    // 2. Destroy all tracked windows (player must die first — it holds PxController)
    if (m_windowSystem) m_windowSystem->ClearAll();
    m_player.reset();

    // 3. Rebuild PhysX from scratch
    m_controllerManager.reset();
    m_scene.reset();
    m_dispatcher.reset();
    m_physics.reset();
    m_foundation.reset();
    InitializePhysics();

    // 4. Rebuild window tracking system
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(k_pixelToUnitRatio);
    m_windowSystem->SetFOV(k_fov);

    // 5. Rebuild camera
    const float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(k_fov), 1920.0f / 1080.0f, k_camNear, k_camFar);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 6. Rebuild player
    m_player = std::make_unique<Player>();
    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get(),
        PlayerConst::CapsuleHalfHeight);  // Kaki tepat di Y=0, gravity off
    m_player->SetGravityEnabled(false);
    m_player->SetInvertControls(false);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);
    m_player->SetMoveSpeed(20.0f);

    // 7. Reset runtime state
    m_timeScale = 1.0f;
    m_spawnCount = 0;
    m_currentStretch = { 0.0f, 0.0f };
    m_stretchOffset = { 0.0f, 0.0f };
    m_showGrid = false;
    m_autoSyncMainWindow = true;
    m_topmostEnabled = true;
    m_playerWindowTransparent = false;
    m_debugLogs.clear();

    WindowManager::Instance().SetTopmost(m_topmostEnabled);
    InitializeSubWindows();

    AddLog("HARD RESET: scene fully rebuilt.");
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

    m_windowSystem->AddTrackedWindow(config,
        []() { return XMFLOAT3(0.0f, 0.0f, 0.0f); });

    TrackedWindow* tracked = m_windowSystem->GetTrackedWindow(config.name);
    if (tracked && tracked->window)
    {
        SDL_SetWindowResizable(tracked->window->GetSDLWindow(), true);
        SDL_SetWindowBordered(tracked->window->GetSDLWindow(), true);
    }

    AddLog("Spawned portal: " + config.name);
    WindowManager::Instance().EnforceWindowPriorities();
}

void SceneBoss::SpawnTransparentWindow(float bgAlpha, const std::string& typeSuffix)
{
    m_spawnCount++;

    TrackedWindowConfig config;
    config.name = "trans_" + typeSuffix + "_" + std::to_string(m_spawnCount);
    config.title = "T-" + typeSuffix + " " + std::to_string(m_spawnCount);
    config.width = 300;
    config.height = 300;
    config.role = WindowRole::SUB_VIEWPORT;
    config.isTransparent = true;

    m_windowSystem->AddTrackedWindow(config,
        []() { return XMFLOAT3(0.0f, 0.0f, 0.0f); });

    TrackedWindow* tracked = m_windowSystem->GetTrackedWindow(config.name);
    if (tracked && tracked->window)
    {
        tracked->window->SetBackgroundAlpha(bgAlpha);
        tracked->window->SetDraggable(true);
        tracked->window->SetBorderVisible(true);
    }

    AddLog("Spawned " + typeSuffix + " window (alpha: " + std::to_string(bgAlpha) + ")");
}

void SceneBoss::CloseSubWindowBySDLID(Uint32 sdlWindowID)
{
    if (!m_windowSystem) return;

    for (const auto& tracked : m_windowSystem->GetWindows())
    {
        if (tracked->window &&
            SDL_GetWindowID(tracked->window->GetSDLWindow()) == sdlWindowID)
        {
            const std::string name = tracked->name;
            m_windowSystem->RemoveTrackedWindow(name);
            AddLog("Window closed: " + name);
            return;
        }
    }
}