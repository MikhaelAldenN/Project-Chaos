// SceneGameBeyond_Refactored.cpp
#include <imgui.h>
#include <SDL3/SDL.h>
#include <cmath>
#include <windows.h>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGameBeyond.h"
#include "WindowManager.h"
#include "Framework.h"

using namespace DirectX;

// =========================================================
// CONSTRUCTOR
// =========================================================
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
    m_player->SetPosition(0.0f, 0.0f, -8.0f);
    m_player->SetInvertControls(true);

    m_blockManager = std::make_unique<BlockManager>();
    m_blockManager->Initialize(m_player.get());
    m_blockManager->ClearBlocks();
    m_blockManager->ActivateFormationMode();

    for (int i = 0; i < 20; ++i) m_blockManager->SpawnAllyBlock(m_player.get());

    m_boss = std::make_unique<Boss>();

    // 3. Sub Camera
    m_subCamera = std::make_shared<Camera>();
    m_subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    m_subCamera->SetPosition(5, 5, 5);
    m_subCamera->LookAt({ 0, 0, 0 });

    // 4. Setup Primitive Renderer
    auto device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
}

// =========================================================
// DESTRUCTOR
// =========================================================
SceneGameBeyond::~SceneGameBeyond()
{
    WindowShatterManager::Instance().Clear();
    CameraController::Instance().ClearCamera();

    // Clean up all tracked windows
    for (auto& tracked : m_trackedWindows)
    {
        if (tracked->window)
        {
            WindowManager::Instance().DestroyWindow(tracked->window);
        }
    }
    m_trackedWindows.clear();
    m_windowLookup.clear();
}

// =========================================================
// INITIALIZE SUB WINDOWS - SIMPLIFIED!
// =========================================================
void SceneGameBeyond::InitializeSubWindows()
{
    // =====================================================
    // KONFIGURASI WINDOWS - EDIT DI SINI!
    // =====================================================

    // 1. Player Tracking Window
    AddTrackedWindow(
        { "player", "Player View", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },
        [this]() { return m_player->GetPosition(); }
    );

    // 2. Boss Monitor 1 (Main Head)
    if (m_boss->HasPart("monitor1"))
    {
        AddTrackedWindow(
            { "monitor1", "Boss Monitor", 340, 340, 0, { -0.3f, 0.0f, 2.1f } },
            [this]() {
                auto pos = m_boss->GetMonitorVisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    // 3. CPU (Body)
    if (m_boss->HasPart("cpu"))
    {
        AddTrackedWindow(
            { "cpu", "System Unit", 186, 370, 0, { -8.2f, 0.0f, 4.0f } },
            [this]() {
                auto pos = m_boss->GetCPUVisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    // 4. Monitor 2 (Side Left)
    if (m_boss->HasPart("monitor2"))
    {
        AddTrackedWindow(
            { "monitor2", "Side Monitor L", 240, 210, 4, { 0.5f, 0.0f, -0.3f } },
            [this]() {
                auto pos = m_boss->GetMonitor2VisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    // 5. Monitor 3 (Side Right) - BARU! TINGGAL TAMBAH DI SINI!
    if (m_boss->HasPart("monitor3"))
    {
        AddTrackedWindow(
            { "monitor3", "Side Monitor R", 200, 200, 3, { 0.8f, 0.0f, 1.2f } },
            [this]() {
                auto pos = m_boss->GetMonitor3VisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    // CARA TAMBAH WINDOW BARU:
    // Tinggal copy paste block AddTrackedWindow di atas!
    // Ganti name, title, size, priority, offset, dan lambda function

    /*
    // Contoh: Keyboard Window
    if (m_boss->HasPart("keyboard"))
    {
        AddTrackedWindow(
            { "keyboard", "Keyboard", 400, 150, 5, { 0.0f, 0.0f, 0.0f } },
            [this]() {
                auto pos = m_boss->GetPartVisualPos("keyboard");
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }
    */

    WindowManager::Instance().EnforceWindowPriorities();
    m_isWindowsInitialized = true;
}

// =========================================================
// ADD TRACKED WINDOW - Helper Function
// =========================================================
bool SceneGameBeyond::AddTrackedWindow(
    const TrackedWindowConfig& config,
    std::function<DirectX::XMFLOAT3()> getTargetPos)
{
    // Create window
    GameWindow* window = WindowManager::Instance().CreateGameWindow(
        config.title.c_str(),
        config.width,
        config.height
    );

    if (!window)
    {
        char msg[256];
        sprintf_s(msg, "❌ Failed to create window: %s\n", config.name.c_str());
        OutputDebugStringA(msg);
        return false;
    }

    window->SetPriority(config.priority);
    if (config.name == "player") window->SetDraggable(false);

    // Create camera
    auto camera = std::make_shared<Camera>();
    window->SetCamera(camera.get());

    // Create tracked window object
    auto tracked = std::make_unique<TrackedWindow>();
    tracked->name = config.name;
    tracked->window = window;
    tracked->camera = camera;
    tracked->trackingOffset = config.trackingOffset;
    tracked->getTargetPositionFunc = getTargetPos;

    // Initialize position
    if (getTargetPos)
    {
        XMFLOAT3 initialPos = getTargetPos();
        float screenX, screenY;
        WorldToScreenPos(initialPos, screenX, screenY);

        tracked->state.targetX = screenX - (window->GetWidth() * 0.5f);
        tracked->state.targetY = screenY - (window->GetHeight() * 0.5f);
        tracked->state.actualX = static_cast<int>(roundf(tracked->state.targetX));
        tracked->state.actualY = static_cast<int>(roundf(tracked->state.targetY));

        SDL_SetWindowPosition(window->GetSDLWindow(),
            tracked->state.actualX,
            tracked->state.actualY);
    }

    // Add to lookup
    m_windowLookup[config.name] = tracked.get();

    // Add to vector
    m_trackedWindows.push_back(std::move(tracked));

    char msg[256];
    sprintf_s(msg, "✓ Created tracked window: %s\n", config.name.c_str());
    OutputDebugStringA(msg);

    return true;
}

// =========================================================
// GET TRACKED WINDOW
// =========================================================
TrackedWindow* SceneGameBeyond::GetTrackedWindow(const std::string& name)
{
    auto it = m_windowLookup.find(name);
    if (it != m_windowLookup.end()) return it->second;
    return nullptr;
}

// =========================================================
// UPDATE ALL TRACKED WINDOWS - SIMPLIFIED!
// =========================================================
void SceneGameBeyond::UpdateAllTrackedWindows(float dt)
{
    for (auto& tracked : m_trackedWindows)
    {
        UpdateSingleWindow(dt, *tracked);
    }
}

// =========================================================
// UPDATE SINGLE WINDOW
// =========================================================
void SceneGameBeyond::UpdateSingleWindow(float dt, TrackedWindow& tracked)
{
    if (!tracked.window || !tracked.camera || !tracked.getTargetPositionFunc)
        return;

    // Get target position dari lambda function
    XMFLOAT3 targetWorldPos = tracked.getTargetPositionFunc();

    // Apply offset
    targetWorldPos.x += tracked.trackingOffset.x;
    targetWorldPos.y += tracked.trackingOffset.y;
    targetWorldPos.z += tracked.trackingOffset.z;

    // Convert to screen
    float targetScreenX, targetScreenY;
    WorldToScreenPos(targetWorldPos, targetScreenX, targetScreenY);

    float destX = targetScreenX - (tracked.window->GetWidth() * 0.5f);
    float destY = targetScreenY - (tracked.window->GetHeight() * 0.5f);

    // Smooth lerp
    float t = min(m_windowFollowSpeed * dt, 1.0f);
    tracked.state.targetX += (destX - tracked.state.targetX) * t;
    tracked.state.targetY += (destY - tracked.state.targetY) * t;

    // Convert to integer
    int newX = static_cast<int>(roundf(tracked.state.targetX));
    int newY = static_cast<int>(roundf(tracked.state.targetY));

    // Threshold untuk reduce SDL calls
    static constexpr int MIN_MOVEMENT_THRESHOLD = 2;

    int deltaX = abs(newX - tracked.state.actualX);
    int deltaY = abs(newY - tracked.state.actualY);

    if (deltaX >= MIN_MOVEMENT_THRESHOLD || deltaY >= MIN_MOVEMENT_THRESHOLD)
    {
        SDL_SetWindowPosition(tracked.window->GetSDLWindow(), newX, newY);
        tracked.state.actualX = newX;
        tracked.state.actualY = newY;
    }

    // Update projection
    UpdateOffCenterProjection(
        tracked.camera.get(),
        tracked.window,
        GetUnifiedCameraHeight()
    );
}

// =========================================================
// UPDATE LOOP - SIMPLIFIED!
// =========================================================
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

    // 1. Wait for Enter to start
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

    // 2. Deferred window initialization
    if (!m_isWindowsInitialized)
    {
        m_startupTimer += elapsedTime;
        if (m_startupTimer > DEFERRED_INIT_TIME)
        {
            InitializeSubWindows();
        }
        return; // Exit early
    }

    // 3. Update Managers
    WindowShatterManager::Instance().Update(elapsedTime);

    // Update Shatter Projections
    float unifiedHeight = GetUnifiedCameraHeight();
    for (const auto& shatter : WindowShatterManager::Instance().GetShatters())
    {
        if (shatter->IsNativeWindow())
        {
            UpdateOffCenterProjection(
                shatter->GetCamera(),
                shatter->GetWindow(),
                unifiedHeight
            );
        }
    }

    // 4. Game Logic
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

    // 5. Update ALL tracked windows (SIMPLE!)
    UpdateAllTrackedWindows(elapsedTime);

    // 6. Throttled priority enforcement
    m_priorityEnforceTimer += elapsedTime;
    if (m_priorityEnforceTimer >= PRIORITY_ENFORCE_INTERVAL)
    {
        WindowManager::Instance().EnforceWindowPriorities();
        m_priorityEnforceTimer = 0.0f;
    }

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}

// =========================================================
// DRAW GUI
// =========================================================
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

        ImGui::Separator();
        ImGui::Text("Tracked Windows: %d", (int)m_trackedWindows.size());

        // Edit individual window settings
        for (auto& tracked : m_trackedWindows)
        {
            ImGui::PushID(tracked->name.c_str());

            if (ImGui::TreeNode(tracked->name.c_str()))
            {
                ImGui::DragFloat3("Offset", &tracked->trackingOffset.x, 0.1f);

                int size[2] = {
                    tracked->window->GetWidth(),
                    tracked->window->GetHeight()
                };

                if (ImGui::DragInt2("Size", size, 1, 100, 1000))
                {
                    SDL_SetWindowSize(tracked->window->GetSDLWindow(), size[0], size[1]);
                    tracked->window->Resize(size[0], size[1]);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
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
            m_textConfig = Text3DConfig();
        }
    }

    // Shatter overlay
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const auto& shatters = WindowShatterManager::Instance().GetShatters();

    for (const auto& shatter : shatters)
    {
        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            float screenX, screenY;
            WorldToScreenPos(worldPos, screenX, screenY);

            drawList->AddRectFilled(
                ImVec2(screenX - size.x * 0.5f, screenY - size.y * 0.5f),
                ImVec2(screenX + size.x * 0.5f, screenY + size.y * 0.5f),
                IM_COL32(255, 255, 255, 255)
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
    }

    ImGui::End();
}

// =========================================================
// HELPER FUNCTIONS (Same as before)
// =========================================================
void SceneGameBeyond::GetScreenDimensions(int& outWidth, int& outHeight) const
{
    if (m_cachedScreenWidth > 0 && m_cachedScreenHeight > 0)
    {
        outWidth = m_cachedScreenWidth;
        outHeight = m_cachedScreenHeight;
        return;
    }

    outWidth = GetSystemMetrics(SM_CXSCREEN);
    outHeight = GetSystemMetrics(SM_CYSCREEN);
    m_cachedScreenWidth = outWidth;
    m_cachedScreenHeight = outHeight;
}

void SceneGameBeyond::WorldToScreenPos(const DirectX::XMFLOAT3& worldPos,
    float& outScreenX, float& outScreenY) const
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

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

void SceneGameBeyond::UpdateOffCenterProjection(Camera* targetCam,
    GameWindow* targetWin, float camHeight)
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

    targetCam->SetPosition(0.0f, camHeight, 0.0f);
    targetCam->LookAt({ 0.0f, 0.0f, 0.0f });

    int winX, winY;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);

    int winW, winH;
    SDL_GetWindowSize(targetWin->GetSDLWindow(), &winW, &winH);

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) * 0.5f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

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


//void SceneGameBeyond::UpdateLensLogic()
//{
//    if (!m_lensWindow || !m_lensCamera) return;
//    UpdateOffCenterProjection(m_lensCamera.get(), m_lensWindow, GetUnifiedCameraHeight());
//}

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