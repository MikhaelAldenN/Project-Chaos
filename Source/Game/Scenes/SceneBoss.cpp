#include "SceneBoss.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "WindowManager.h"
#include "Framework.h"
#include "InputHelper.h"
#include "PerformanceLogger.h"
#include <algorithm>
#include <imgui.h>
#include "System/CollisionManager.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Stage.h"
#include "Boss.h"
#include <random>
#include "NaviPhaseWindowkill.h"
#include "NaviPhaseNormal.h"
#include "TimeManager.h"

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
    m_mainCamera->SetPosition(m_cameraPosition);
    m_mainCamera->LookAt(m_cameraTarget);

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
	m_player->SetDashSpeed(60.0f);

    //// --- TAMBAHKAN INISIALISASI MANAGER DI SINI ---
    //auto device = Graphics::Instance().GetDevice();

    // --- Primitive Renderers ---
    ID3D11Device* device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
    m_primitive3D = std::make_unique<PrimitiveRenderer>(device);

    m_stage = std::make_unique<Stage>(device); // Walau kosong, ini mencegah Null Pointer

    //m_enemyManager = std::make_unique<EnemyManager>();
    //m_enemyManager->Initialize(device);

    //m_itemManager = std::make_unique<ItemManager>();
    //m_itemManager->Initialize(device);

    // Jika Anda sudah memiliki inisialisasi Boss, panggil di sini
    // m_boss = std::make_unique<Boss>(); 

    m_collisionManager = std::make_unique<CollisionManager>();

    // Gunakan Overload 2 yang ada Boss-nya
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), m_enemyManager.get(), m_itemManager.get(), m_boss.get());

    // PENTING: Beri tahu Player siapa wasit (CollisionManager) di scene ini!
    m_player->SetCollisionManager(m_collisionManager.get());

    m_navi = std::make_unique<NaviBoss>();
    m_navi->Initialize(m_windowSystem.get());

#if 0
    m_navi->ChangePhase(std::make_unique<NaviPhaseWindowkill>());
#else
    m_navi->ChangePhase(std::make_unique<NaviPhaseNormal>());
#endif

    if (m_collisionManager) {
        m_collisionManager->SetNaviBoss(m_navi.get());
    }

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
    TimeManager::Instance().Update(elapsedTime);

    float activeTimeScale = m_timeScale * TimeManager::Instance().GetHitStopMultiplier();
    const float scaledDt = elapsedTime * activeTimeScale;

    // --- PhysX tick ---
    if (m_scene)
    {
        m_scene->simulate(scaledDt);
        m_scene->fetchResults(true);
    }

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    if (m_navi && dynamic_cast<NaviPhaseWindowkill*>(m_navi->GetCurrentPhase())) {
        m_autoSyncMainWindow = true;
    }

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

    // =========================================================
    // KAMERA STATIS (NO ZOOM)
    // =========================================================
    m_targetZoom = 0.0f;
    m_currentZoom = 0.0f;

    // Kunci Rasio Piksel ke default agar ukuran dunia stabil
    float dynamicPixelRatio = k_pixelToUnitRatio;
    m_windowSystem->SetPixelToUnitRatio(dynamicPixelRatio);

    // =========================================================
    // [FIX] UPDATE KAMERA & CAMERA SHAKE (GABUNGAN)
    // =========================================================
    // Deklarasi hanya dilakukan SATU KALI di sini!
    float newUnifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    auto& camCtrl = CameraController::Instance();

    // Ambil nilai getaran (Trauma)
    DirectX::XMFLOAT3 shake = camCtrl.GetShakeOffset();
    
    // Set posisi kamera dengan menggabungkan Zoom (Y) dan Shake (X, Z)
    camCtrl.SetFixedSetting(DirectX::XMFLOAT3(shake.x, newUnifiedHeight + shake.y, shake.z));
    camCtrl.SetTarget({ shake.x, shake.y, shake.z }); 
    
    // [PENTING] Update kamera menggunakan waktu murni (elapsedTime) agar tetap bergetar saat Hit Stop!
    camCtrl.Update(elapsedTime);
    // =========================================================

    // --- Player update ---
    if (m_player)
    {
        const XMFLOAT3 mouseWorldPos = Beyond::InputHelper::GetMouseWorldPos(m_mainCamera->GetPosition());
        m_player->RotateModelToPoint(mouseWorldPos);
        m_player->Update(scaledDt, activeCam);

        // =========================================================
        // [CRITICAL FIX] DYNAMIC SCREEN CLAMPING
        // Kita kunci player agar SELALU ada di dalam layar yang sedang ter-zoom
        // =========================================================
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // Gunakan dynamicPixelRatio agar batas layar bergeser sesuai Zoom!
        float limitX = ((screenWidth / 2.0f) / dynamicPixelRatio) - 0.5f; // Margin 0.5 agar tidak nempel bgt
        float limitZ = ((screenHeight / 2.0f) / dynamicPixelRatio) - 0.5f;

        XMFLOAT3 pos = m_player->GetPosition();
        pos.x = std::clamp(pos.x, -limitX, limitX);
        pos.z = std::clamp(pos.z, -limitZ, limitZ);
        pos.y = max(pos.y, 0.0f);
        m_player->SetPosition(pos);
    }

    // --- Squash & Stretch ---
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
            constexpr float kSquashY = 0.0f;
            constexpr float kStretchZ = 0.0f;
            constexpr float kSquashX = 0.0f;

            const float dashRatio = sqrtf(currentSpeedSq) / m_player->GetDashSpeed();

            if (std::abs(vel.x) > std::abs(vel.z))
            {
                targetStretch.x = dashRatio * kStretchX;
                targetStretch.y = dashRatio * kSquashY;
                const float signX = (vel.x > 0.0f) ? 1.0f : -1.0f;
                // Gunakan dynamicPixelRatio
                targetOffset.x = -signX * (targetStretch.x * 0.5f) / dynamicPixelRatio;
            }
            else
            {
                targetStretch.y = dashRatio * kStretchZ;
                targetStretch.x = dashRatio * kSquashX;
                const float signZ = (vel.z > 0.0f) ? 1.0f : -1.0f;
                // Gunakan dynamicPixelRatio
                targetOffset.y = -signZ * (targetStretch.y * 0.5f) / dynamicPixelRatio;
            }
        }

        m_currentStretch.x += (targetStretch.x - m_currentStretch.x) * lerpSpeed * scaledDt;
        m_currentStretch.y += (targetStretch.y - m_currentStretch.y) * lerpSpeed * scaledDt;
        m_stretchOffset.x += (targetOffset.x - m_stretchOffset.x) * lerpSpeed * scaledDt;
        m_stretchOffset.y += (targetOffset.y - m_stretchOffset.y) * lerpSpeed * scaledDt;
    }

    // --- Entities & Collision Update ---
    if (m_navi) {
        // [FIX] Oper data Player ke AI Director sebelum Update berjalan
        if (auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_navi->GetCurrentPhase())) {
            normalPhase->SetAITarget(m_player.get());
        }
        m_navi->Update(scaledDt);
    }
    if (m_enemyManager) m_enemyManager->Update(scaledDt, activeCam, m_player->GetPosition(), true);
    if (m_itemManager) m_itemManager->Update(scaledDt, activeCam);
    if (m_collisionManager) m_collisionManager->Update(scaledDt);

    // Terapkan posisi m_fixedPos dan Shakes
    //camCtrl.Update(scaledDt);
    
    // --- Sync sub-window cameras to match main camera ---
    if (m_windowSystem)
    {
        for (auto& tracked : m_windowSystem->GetWindows())
        {
            if (!tracked->camera || tracked->camera == m_mainCamera) continue;

            tracked->camera->SetPosition(m_mainCamera->GetPosition());
            tracked->camera->SetRotation(m_mainCamera->GetRotation());

            //if (tracked->role != WindowRole::SUB_VIEWPORT && m_player)
            //    tracked->camera->LookAt(m_player->GetPosition());
        }

        m_windowSystem->Update(elapsedTime);
    }

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

    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
    if (m_showHitboxes)
    {
        // 1. Gambar Hurtbox Player (Lingkaran Hijau)
                // Kita kunci di y=1.0f agar terlihat menonjol di badan player
        if (m_player && m_player->GetHP() > 0) {
            DirectX::XMFLOAT3 pPos = m_player->GetPosition();
            pPos.y = 1.0f;
            shapeRenderer->DrawSphere(pPos, 0.3f, { 0.0f, 1.0f, 0.0f, 1.0f });
        }

        // 2. Gambar Hitbox Peluru Navi (Lingkaran Merah)
        if (m_navi) {
            if (auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_navi->GetCurrentPhase())) {
                for (auto& bullet : normalPhase->GetProjectiles()) {
                    if (bullet->IsActive()) {
                        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
                        shapeRenderer->DrawSphere(bPos, bullet->GetRadius(), { 1.0f, 0.0f, 0.0f, 1.0f });
                    }
                }
            }
        }
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

    // --- 1. DETEKSI KAMERA SAYAP (CAMERA FILTERING) ---
    bool isWingCamera = false;
    if (m_navi) {
        // Tanya ke sistem: "Apakah fase saat ini adalah Windowkill?"
        if (auto* wkPhase = dynamic_cast<NaviPhaseWindowkill*>(m_navi->GetCurrentPhase())) {
            // Jika iya, ambil kameranya!
            isWingCamera = (camera == wkPhase->GetFXCamera());
        }
    }

    // --- 2. RENDER ENTITAS UMUM (Hanya jika BUKAN kamera sayap) ---
    if (!isWingCamera)
    {
        if (m_player)
        {
            const XMFLOAT3 pPos = m_player->GetPosition();
            if (camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f))
            {
                m_player->Render(modelRenderer);
            }
            m_player->RenderProjectiles(modelRenderer);
        }

        if (m_enemyManager) m_enemyManager->Render(modelRenderer);
        if (m_itemManager) m_itemManager->Render(modelRenderer);
    }

    // --- 3. RENDER NAVI ---
    // Navi dipanggil di semua kamera, tetapi di dalam NaviBoss::Render 
    // sudah ada filter internal agar bagian badannya tidak tertukar.
    if (m_navi) m_navi->Render(dc, camera);

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

    // =========================================================
    // MULAI TAB BAR
    // =========================================================
    if (ImGui::BeginTabBar("MasterControlTabs"))
    {
        // ---------------------------------------------------------
        // TAB 1: SYSTEM & ENGINE (Berisi Metrics, Window Config, World)
        // ---------------------------------------------------------
        if (ImGui::BeginTabItem("System & Engine"))
        {
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

            if (ImGui::CollapsingHeader("Window Tracking Config", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Checkbox("[All] Toggle Topmost (triggers reset)", &m_topmostEnabled))
                {
                    WindowManager::Instance().SetTopmost(m_topmostEnabled);
                    ResetEverything();
                }

                // Cek apakah m_navi ada, dan apakah fasenya adalah Windowkill
                if (m_navi) {
                    if (auto* wkPhase = dynamic_cast<NaviPhaseWindowkill*>(m_navi->GetCurrentPhase())) {
                        bool fxClickthrough = wkPhase->IsFXClickThrough();
                        if (ImGui::Checkbox("[ALL] Toggle Clickthrough", &fxClickthrough)) {
                            wkPhase->SetFXClickThrough(fxClickthrough);
                            AddLog(fxClickthrough ? "FX Window: Click-through Enabled" : "FX Window: Click-through Disabled");
                        }
                    }
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
                        [this]() -> DirectX::XMFLOAT3 {
                            if (!m_player) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
                            const auto pPos = m_player->GetPosition();
                            return DirectX::XMFLOAT3(
                                pPos.x + m_stretchOffset.x,
                                0.0f,
                                pPos.z + m_stretchOffset.y
                            );
                        },
                        [this]() -> DirectX::XMFLOAT2 {
                            return DirectX::XMFLOAT2(
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

            if (ImGui::CollapsingHeader("World & Entities", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Show 3D Grid", &m_showGrid);
                ImGui::Checkbox("Show Hitboxes/Hurtboxes", &m_showHitboxes);

                if (m_player)
                {
                    const DirectX::XMFLOAT3 pPos = m_player->GetPosition();
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

            if (ImGui::CollapsingHeader("Cinematic Camera", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Combat Radius", &m_combatRadius, 5.0f, 50.0f);
                ImGui::SliderFloat("Max Zoom Amount", &m_maxZoomIn, -15.0f, 0.0f);
                ImGui::Text("Current Zoom: %.2f", m_currentZoom);

                if (ImGui::Button("Reset Zoom Settings")) {
                    m_combatRadius = 25.0f;
                    m_maxZoomIn = -8.0f;
                }
            }

            ImGui::EndTabItem();
        }

        // ---------------------------------------------------------
        // TAB 2: NAVI BOSS
        // ---------------------------------------------------------
        if (m_navi && ImGui::BeginTabItem("Navi Boss"))
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Core Settings ---");
            float speed = m_navi->GetCoreBreathSpeed();
            float intensity = m_navi->GetCoreBreathIntensity();

            bool changed = false;
            changed |= ImGui::SliderFloat("Breath Speed", &speed, 0.1f, 20.0f);
            changed |= ImGui::SliderFloat("Breath Intensity", &intensity, 0.0f, 200.0f);
            if (changed) m_navi->SetCoreBreathParams(speed, intensity);

            // =========================================================
            // [MAGIC] DYNAMIC CAST UNTUK KONTROL SAYAP
            // Jika Fase Normal sedang aktif, UI sayap di bawah ini akan 
            // otomatis hilang (disembunyikan) agar UI tetap rapi!
            // =========================================================
            if (auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_navi->GetCurrentPhase()))
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "--- PHASE 1: NORMAL MODE ---");
                ImGui::TextWrapped("Main Window is now in Borderless Fullscreen.");

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));

                // TOMBOL DEBUG TRANSISI!
                if (ImGui::Button("TRIGGER PHASE 2 (WINDOWKILL) !!!", ImVec2(-1.0f, 50.0f))) {
                    m_navi->ChangePhase(std::make_unique<NaviPhaseWindowkill>());
                    AddLog("Transitioning to Windowkill Phase...");
                }

                ImGui::PopStyleColor(2);
            }

            // --- JIKA SEDANG DI FASE 2 (WINDOWKILL) ---
            else if (auto* wkPhase = dynamic_cast<NaviPhaseWindowkill*>(m_navi->GetCurrentPhase()))
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Wing Settings ---");

                float wSpeed = wkPhase->GetWingFlapSpeed();
                float wIntensity = wkPhase->GetWingFlapIntensity();
                float wOffsetX = wkPhase->GetWingOffsetX();
                float wOffsetZ = wkPhase->GetWingOffsetZ();
                bool offsetChanged = false;

                if (ImGui::SliderFloat("Wing Flap Speed", &wSpeed, 0.1f, 10.0f)) wkPhase->SetWingFlapParams(wSpeed, wIntensity);
                if (ImGui::SliderFloat("Wing Flap Intensity", &wIntensity, 0.0f, 2.0f)) wkPhase->SetWingFlapParams(wSpeed, wIntensity);

                offsetChanged |= ImGui::SliderFloat("Wing Spacing (X)", &wOffsetX, 0.0f, 20.0f);
                offsetChanged |= ImGui::SliderFloat("Wing Vertical (Z)", &wOffsetZ, -20.0f, 20.0f);
                if (offsetChanged) wkPhase->SetWingOffsets(wOffsetX, wOffsetZ);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Render Scale ---");

                float p2u = wkPhase->GetPixelToUnit();
                float gScale = wkPhase->GetWingGlobalScale();

                if (ImGui::SliderFloat("Pixel to Unit Ratio", &p2u, 1.0f, 100.0f)) wkPhase->SetScalingParams(p2u, gScale);
                if (ImGui::SliderFloat("Global Wing Scale", &gScale, 0.1f, 5.0f)) wkPhase->SetScalingParams(p2u, gScale);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Procedural Generation ---");

                int currentSeed = static_cast<int>(wkPhase->GetWingSeed());
                if (ImGui::InputInt("Wing Seed", &currentSeed)) {
                    wkPhase->SetWingSeed(static_cast<unsigned int>(currentSeed));
                }

                if (ImGui::Button("Randomize Seed (Gacha!)", ImVec2(-1.0f, 30.0f))) {
                    std::random_device rd;
                    wkPhase->SetWingSeed(rd());
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "--- Spawn Animation ---");

                float pDur = wkPhase->GetPopDuration();
                float sDur = wkPhase->GetSpawnDuration();
                float sChaos = wkPhase->GetSpawnChaos();
                bool animChanged = false;

                animChanged |= ImGui::SliderFloat("Individual Pop Duration", &pDur, 0.01f, 1.0f);
                animChanged |= ImGui::SliderFloat("Total Spawn Duration", &sDur, 0.1f, 5.0f);
                animChanged |= ImGui::SliderFloat("Spawn Chaos", &sChaos, 0.0f, 2.0f);

                if (animChanged) wkPhase->SetSpawnParams(pDur, sDur, sChaos);

                if (ImGui::Button("Re-play Expand Animation", ImVec2(-1.0f, 30.0f))) {
                    wkPhase->ReplayAnimation();
                }
            }

            ImGui::EndTabItem();
        }

        // =========================================================
        // TAB: BOSS ATTACKS
        // =========================================================
        if (m_navi && ImGui::BeginTabItem("Boss Attacks")) {
            if (auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_navi->GetCurrentPhase())) {
                auto& p = normalPhase->GetParams();

                ImGui::TextColored(ImVec4(1, 1, 0, 1), "--- Global Bullet Settings ---");
                ImGui::SliderFloat("Bullet Speed", &p.speed, 1.0f, 50.0f);
                ImGui::SliderInt("Radial Count", &p.count, 4, 128);
                ImGui::SliderFloat("Burst Delay", &p.burstDelay, 0.01f, 1.0f);
                ImGui::ColorEdit4("Bullet Color", (float*)&p.color);

                // --- UI BARU UNTUK FAN BURST ---
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "--- Targeted Fan Burst Settings ---");
                ImGui::SliderInt("Fan Lines (Bullets/Wave)", &p.fanLines, 1, 10);
                ImGui::SliderInt("Fan Waves (Repeats)", &p.fanWaves, 1, 10);
                ImGui::SliderFloat("Fan Wave Delay", &p.fanWaveDelay, 0.05f, 1.0f);
                ImGui::SliderFloat("Fan Spread Angle", &p.fanSpreadAngle, 0.05f, 0.5f);

				// --- UI BARU UNTUK PHALANX ---1
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "--- Phalanx Settings ---");
                ImGui::SliderInt("Phalanx Count", &p.phalanxCount, 3, 10);
                ImGui::SliderFloat("Smooth Speed (Spawn)", &p.phalanxSmoothSpeed, 1.0f, 20.0f);
                ImGui::SliderFloat("Charge Delay", &p.phalanxChargeDelay, 0.05f, 0.5f);
                ImGui::SliderFloat("Hold Duration", &p.phalanxHoldDuration, 0.0f, 3.0f);
                ImGui::SliderFloat("Fire Delay", &p.phalanxFireDelay, 0.05f, 0.5f);
                ImGui::SliderFloat("Phalanx Speed", &p.phalanxSpeed, 10.0f, 80.0f);
                ImGui::SliderFloat("Phalanx Turn Speed", &p.phalanxTurnSpeed, 0.1f, 10.0f);
                ImGui::SliderFloat("Post-Fire Delay", &p.phalanxPostFireDelay, 0.0f, 3.0f);
                ImGui::SliderFloat("Attack Move Speed", &p.phalanxAttackMoveSpeed, 1.0f, 20.0f);
                ImGui::SliderFloat("Return Move Speed", &p.phalanxReturnMoveSpeed, 0.1f, 10.0f);

                // --- UI BARU UNTUK RHYTHM LASER ---
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "--- Rhythm Laser & Bijuudama Settings ---");
                ImGui::SliderFloat("Laser Duration", &p.laserDuration, 0.5f, 4.0f);
                ImGui::SliderFloat("Parry Window (+/- sec)", &p.laserParryWindow, 0.05f, 0.5f);
                ImGui::SliderInt("Laser Damage", &p.laserDamage, 10, 50);

                // [BARU] Slider Bijuudama
                ImGui::SliderFloat("Base Radius", &p.bijuudamaBaseHitbox, 0.1f, 2.0f);
                ImGui::SliderFloat("Max Grow Amount", &p.bijuudamaMaxHitboxGrow, 0.0f, 10.0f);
                ImGui::SliderFloat("Visual Multiplier", &p.bijuudamaVisualMultiplier, 1.0f, 10.0f);
                ImGui::SliderFloat("Spawn Offset Z", &p.bijuudamaSpawnOffsetZ, 0.0f, 10.0f);
                ImGui::SliderFloat("Shoot Speed", &p.bijuudamaShootSpeed, 10.0f, 120.0f);
                
                ImGui::SliderFloat("Post-Fire Delay", &p.bijuudamaPostFireDelay, 0.0f, 3.0f);
                ImGui::SliderFloat("Move Up Speed", &p.bijuudamaAttackMoveSpeed, 1.0f, 15.0f);
                ImGui::SliderFloat("Return Speed", &p.bijuudamaReturnMoveSpeed, 0.5f, 10.0f);


                // [BARU] Slider Shatter / Pecahan
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "--- Shatter / Parabola Settings ---");
                ImGui::SliderInt("Min Fragments", &p.shatterMinFragments, 1, 10);
                ImGui::SliderInt("Max Fragments", &p.shatterMaxFragments, 1, 20);
                ImGui::SliderFloat("Curve Width/Offset", &p.shatterCurveOffset, 5.0f, 30.0f);
                ImGui::SliderFloat("Min Travel Time", &p.shatterMinDuration, 0.1f, 3.0f);
                ImGui::SliderFloat("Max Travel Time", &p.shatterMaxDuration, 0.1f, 3.0f);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "--- Manual Triggers ---");

                if (ImGui::Button("FIRE TRIPLE BURST", ImVec2(-1.0f, 40.0f))) {
                    normalPhase->TriggerTripleBurst();
                }

                // TOMBOL BARU DENGAN TARGET LOCKING!
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
                if (ImGui::Button("FIRE TARGETED FAN BURST", ImVec2(-1.0f, 40.0f))) {
                    if (m_player) {
                        normalPhase->TriggerFanAttack(m_navi.get(), m_player->GetPosition());
                    }
                }
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
                if (ImGui::Button("FIRE GLINTSTONE PHALANX", ImVec2(-1.0f, 40.0f))) {
                    if (m_player) {
                        normalPhase->TriggerPhalanx(m_player.get());
                    }
                }
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.6f, 1.0f));
                if (ImGui::Button("FIRE BIJUUDAMA (TIMING EVENT)", ImVec2(-1.0f, 40.0f))) {
                    if (m_player) {
                        normalPhase->TriggerBijuudama(m_player.get());
                    }
                }
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.3f, 0.0f, 1.0f));
                if (ImGui::Button("ASGORE RAIN (LEFT)", ImVec2(-1.0f, 30.0f))) {
                    normalPhase->TriggerRainAttack(true, false); // True = Side Mode, False = Left
                }
                if (ImGui::Button("ASGORE RAIN (RIGHT)", ImVec2(-1.0f, 30.0f))) {
                    normalPhase->TriggerRainAttack(true, true);  // True = Side Mode, True = Right
                }
                if (ImGui::Button("ASGORE RAIN (TOP)", ImVec2(-1.0f, 30.0f))) {
                    normalPhase->TriggerRainAttack(false, true); // False = Normal Mode, True = Top
                }
                if (ImGui::Button("ASGORE RAIN (BOTTOM)", ImVec2(-1.0f, 30.0f))) {
                    normalPhase->TriggerRainAttack(false, false);// False = Normal Mode, False = Bottom
                }
                ImGui::PopStyleColor();

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 0, 1, 1), "--- AI DIRECTOR (OMEGA FLOWEY MODE) ---");

                bool aiActive = normalPhase->IsAIEnabled();
                if (ImGui::Checkbox("ENABLE BOSS AI", &aiActive)) {
                    normalPhase->SetAIEnabled(aiActive);
                }
                if (aiActive) {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "WARNING: PURE CHAOS INCOMING! BRACE YOURSELF!");
                }

                ImGui::Separator();
                if (normalPhase->GetHP() > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NAVI BOSS HEALTH: %d / %d", normalPhase->GetHP(), normalPhase->GetMaxHP());
                    float hpProgress = (float)normalPhase->GetHP() / normalPhase->GetMaxHP();
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Bar Merah
                    ImGui::ProgressBar(hpProgress, ImVec2(-1.0f, 30.0f));
                    ImGui::PopStyleColor();
                }
                else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ NAVI DEFEATED ]");
                }

                if (ImGui::CollapsingHeader("Player Vital Signs", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int hp = m_player->GetHP();
                    float hpProgress = hp / 100.0f;

                    // Warna bar: Hijau jika sehat, Merah jika kritis
                    ImVec4 barColor = { (1.0f - hpProgress), hpProgress, 0.0f, 1.0f };

                    ImGui::Text("Player Health: %d / 100", hp);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
                    ImGui::ProgressBar(hpProgress, ImVec2(-1.0f, 25.0f));
                    ImGui::PopStyleColor();

                    if (ImGui::Button("Heal Player (Cheat)", ImVec2(-1.0f, 30.0f))) {
                        m_player->SetMaxHP(100);
                    }

                    if (m_player->IsInvincible()) {
                        ImGui::TextColored(ImVec4(0, 1, 1, 1), "STATUS: I-FRAME ACTIVE (DASHING)");
                    }
                }

            }
            else {
                ImGui::Text("Attacks are only available in Normal Phase.");
            }
            ImGui::EndTabItem();
        }

        // ---------------------------------------------------------
        // TAB 3: TERMINAL
        // ---------------------------------------------------------
        if (ImGui::BeginTabItem("Terminal"))
        {
            ImGui::BeginChild("LogRegion", ImVec2(0.0f, 0.0f), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);
            for (const auto& log : m_debugLogs)
                ImGui::TextUnformatted(log.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
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
    CameraController::Instance().ClearCamera();

    // 1. Destroy everything (Navi harus hancur sebelum WindowSystem)
    m_navi.reset();
    m_collisionManager.reset();
    m_enemyManager.reset();
    m_itemManager.reset();
    m_stage.reset();
    m_boss.reset();
    if (m_windowSystem) m_windowSystem->ClearAll();
    m_player.reset();

    // 2. Rebuild PhysX
    m_defaultMaterial.reset();
    m_controllerManager.reset();
    m_scene.reset();
    m_dispatcher.reset();
    m_physics.reset();
    m_foundation.reset();
    InitializePhysics();

    // 3. Rebuild Window System & Camera
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(k_pixelToUnitRatio);
    m_windowSystem->SetFOV(k_fov);

    const float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(k_fov), 1920.0f / 1080.0f, k_camNear, k_camFar);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 4. Rebuild Player & Managers
    ID3D11Device* device = Graphics::Instance().GetDevice();

    m_player = std::make_unique<Player>();
    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get(), PlayerConst::CapsuleHalfHeight);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);
    m_player->SetMoveSpeed(20.0f);
    m_player->SetDashSpeed(60.0f); // Kembalikan nilai dash

    m_stage = std::make_unique<Stage>(device);

    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), m_enemyManager.get(), m_itemManager.get(), m_boss.get());
    m_player->SetCollisionManager(m_collisionManager.get());

    // =========================================================
    // [FIX] INISIALISASI NAVI BOSS & SET FASE AWAL!
    // =========================================================
    m_navi = std::make_unique<NaviBoss>();
    m_navi->Initialize(m_windowSystem.get());

    // Beri otak ke Navi agar masuk ke Mode Layar Penuh!
    m_navi->ChangePhase(std::make_unique<NaviPhaseNormal>());
    // =========================================================

    m_player->SetMaxHP(100); // Reset darah player
    m_player->scale = { 1.0f, 1.0f, 1.0f }; // Kembalikan badan player jika tadi mati

    if (m_collisionManager) m_collisionManager->SetNaviBoss(m_navi.get());

    // 5. Finalize
    m_timeScale = 1.0f;
    m_spawnCount = 0;
    m_currentStretch = { 0.0f, 0.0f };
    m_stretchOffset = { 0.0f, 0.0f };
    m_showGrid = false;
    m_autoSyncMainWindow = false; // Tetap false agar tidak merusak Fullscreen Fase 1

    //m_topmostEnabled = true;
    m_playerWindowTransparent = false;
    m_debugLogs.clear();

    WindowManager::Instance().SetTopmost(m_topmostEnabled);
    InitializeSubWindows();

    AddLog("HARD RESET: All systems successfully restored.");
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