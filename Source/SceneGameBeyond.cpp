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
    m_player->SetInvertControls(true);

    m_blockManager = std::make_unique<BlockManager>();
    m_blockManager->Initialize(m_player.get());
    m_blockManager->ClearBlocks();
    m_blockManager->ActivateFormationMode();

    for (int i = 0; i < 20; ++i) m_blockManager->SpawnAllyBlock(m_player.get());

    m_boss = std::make_unique<Boss>();
    m_boss->SetPosition(0.0f, 0.6f, 6.5f);
    m_boss->SetRotation(65.0f, 0.0f, 0.0f);
    m_boss->SetScale(10.0f, 10.0f, 10.0f);
    m_boss->SetFloatingConfig(true, 2.0f, 0.2f, { 0.0f, 0.0f, 1.0f });

    // 3. Sub Camera
    m_subCamera = std::make_shared<Camera>();
    m_subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    m_subCamera->SetPosition(5, 5, 5);
    m_subCamera->LookAt({ 0, 0, 0 });

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
}

void SceneGameBeyond::InitializeSubWindows()
{
    // 1. Player Tracking Window
    m_trackingWindow = WindowManager::Instance().CreateGameWindow("Player View", 300, 300);
    m_trackingWindow->SetPriority(1);
    m_trackingWindow->SetDraggable(false);
    m_trackingCamera = std::make_shared<Camera>();
    m_trackingWindow->SetCamera(m_trackingCamera.get());

    // 2. Lens Window
    m_lensWindow = WindowManager::Instance().CreateGameWindow("Lens View", 300, 300);
    m_lensWindow->SetPriority(0);
    m_lensCamera = std::make_shared<Camera>();
    m_lensCamera->SetRotation(90.0f, 0.0f, 0.0f);
    m_lensWindow->SetCamera(m_lensCamera.get());

    // 3. Boss Tracking Window
    m_bossWindow = WindowManager::Instance().CreateGameWindow("Boss Monitor", 370, 370);
    m_bossWindow->SetPriority(2); // Slightly below player
    m_bossWindow->SetDraggable(false);
    m_bossCamera = std::make_shared<Camera>();
    m_bossWindow->SetCamera(m_bossCamera.get());

    // Center Initial Positions
    auto CenterWindow = [&](GameWindow* win, const XMFLOAT3& pos, WindowPos& outPos) {
        float screenX, screenY;
        WorldToScreenPos(pos, screenX, screenY);
        outPos.x = screenX - (win->GetWidth() * 0.5f);
        outPos.y = screenY - (win->GetHeight() * 0.5f);
        SDL_SetWindowPosition(win->GetSDLWindow(), (int)outPos.x, (int)outPos.y);
        };

    if (m_player) CenterWindow(m_trackingWindow, m_player->GetPosition(), m_playerWinPos);
    if (m_boss) CenterWindow(m_bossWindow, m_boss->GetPosition(), m_bossWinPos);

    if (auto mainWin = Framework::Instance()->GetMainWindow()) mainWin->SetVisible(false);

    WindowManager::Instance().EnforceWindowPriorities();
    m_isWindowsInitialized = true;
}

// =========================================================
// SCREEN MATH UTILITIES (DRY Principle)
// =========================================================
void SceneGameBeyond::GetScreenDimensions(int& outWidth, int& outHeight) const
{
    outWidth = GetSystemMetrics(SM_CXSCREEN);
    outHeight = GetSystemMetrics(SM_CYSCREEN);
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
void SceneGameBeyond::UpdateWindowTracking(float dt, GameWindow* win, Camera* cam, const DirectX::XMFLOAT3& targetPos, WindowPos& winPos)
{
    if (!win || !cam) return;

    // 1. Calculate Target Screen Position
    float targetScreenX, targetScreenY;
    WorldToScreenPos(targetPos, targetScreenX, targetScreenY);

    float destX = targetScreenX - (win->GetWidth() * 0.5f);
    float destY = targetScreenY - (win->GetHeight() * 0.5f);

    // 2. Smooth Lerp
    float t = min(m_windowFollowSpeed * dt, 1.0f);
    winPos.x += (destX - winPos.x) * t;
    winPos.y += (destY - winPos.y) * t;

    // 3. Apply to Window
    SDL_SetWindowPosition(win->GetSDLWindow(), static_cast<int>(winPos.x), static_cast<int>(winPos.y));

    // 4. Update Perspective Match
    UpdateOffCenterProjection(cam, win, GetUnifiedCameraHeight());
}

void SceneGameBeyond::Update(float elapsedTime)
{
    // --- INTRO: Shatter Explosion ---
    if (!m_shatterSpawned)
    {
        m_shatterTimer += elapsedTime;
        if (m_shatterTimer >= m_shatterDelay && m_player)
        {
            auto pPos = m_player->GetPosition();
            m_shatterSpawnPos = { pPos.x, pPos.z + 2.0f };

            m_shatterToSpawn = 8; // Set jumlah pecahan
            m_shatterSpawned = true; // Tandai sudah trigger

            // RESET Timer untuk dipakai sebagai interval antar-spawn
            m_shatterTimer = 0.0f;
        }
    }
    else if (m_shatterToSpawn > 0) // Gunakan ELSE IF agar tidak langsung spawn di frame yang sama
    {
        // Pakai Timer lagi agar tidak spawn setiap frame (Beri jeda 0.05 detik)
        m_shatterTimer += elapsedTime;
        if (m_shatterTimer > 0.05f)
        {
            WindowShatterManager::Instance().SpawnSingleShatter(m_shatterSpawnPos, m_shatterToSpawn);
            m_shatterToSpawn--;
            m_shatterTimer = 0.0f; // Reset timer lagi
        }
    }

    WindowShatterManager::Instance().Update(elapsedTime);

    // Update Shatter Projections
    float unifiedHeight = GetUnifiedCameraHeight();
    for (const auto& shatter : WindowShatterManager::Instance().GetShatters())
    {
        if (!shatter->IsNativeWindow()) continue;
        UpdateOffCenterProjection(shatter->GetCamera(), shatter->GetWindow(), unifiedHeight);
    }

    // --- DEFERRED WINDOW INIT ---
    if (!m_isWindowsInitialized)
    {
        m_startupTimer += elapsedTime;
        if (m_startupTimer > DEFERRED_INIT_TIME) InitializeSubWindows();
        else return;
    }

    // --- GAME LOGIC ---
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
        if (Input::Instance().GetKeyboard().IsTriggered('R')) m_blockManager->SpawnAllyBlock(m_player.get());
    }

    // --- WINDOW TRACKING ---
    if (m_player)
    {
        UpdateWindowTracking(elapsedTime, m_trackingWindow, m_trackingCamera.get(), m_player->GetPosition(), m_playerWinPos);
    }

    if (m_boss)
    {
        DirectX::XMFLOAT3 visualPos = m_boss->GetVisualPosition();
        visualPos.x += m_bossTrackingOffset.x;
        visualPos.y += m_bossTrackingOffset.y;
        visualPos.z += m_bossTrackingOffset.z;

        UpdateWindowTracking(elapsedTime, m_bossWindow, m_bossCamera.get(), visualPos, m_bossWinPos);
    }

    UpdateLensLogic();
    WindowManager::Instance().EnforceWindowPriorities();

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}

// =========================================================
// RENDER & GUI
// =========================================================
void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

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

    // Render Grid
    primRenderer->DrawGrid(20, 1);
    primRenderer->Render(dc, camera->GetView(), camera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    // Distance Culling
    DirectX::XMFLOAT3 camPos = camera->GetPosition();
    float cullRadiusSq = 40.0f * 40.0f;
    auto IsVisible = [&](const DirectX::XMFLOAT3& objPos) -> bool {
        float dx = camPos.x - objPos.x;
        float dz = camPos.z - objPos.z;
        return (dx * dx + dz * dz) < cullRadiusSq;
        };

    // Render Entities
    if (m_player) m_player->Render(modelRenderer);
    if (m_boss) m_boss->Render(modelRenderer);
    if (m_blockManager)
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (block->IsActive() && IsVisible(block->GetMovement()->GetPosition()))
                block->Render(modelRenderer, m_blockManager->globalBlockColor);
        }
    }

// --- RENDER FAKE SHATTERS ---
    const auto& shatters = WindowShatterManager::Instance().GetShatters();
    for (const auto& shatter : shatters)
    {
        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetFakeWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetCurrentSize();

            float screenX, screenY;
            WorldToScreenPos(worldPos, screenX, screenY);

            // Gunakan m_primitive2D yang baru kita buat
            m_primitive2D->Rect(
                screenX, screenY,
                size.x, size.y,
                size.x * 0.5f, size.y * 0.5f,
                0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            );
        }
    }

    m_primitive2D->Render(dc);

    modelRenderer->Render(rc);

    // Render Boss 3D Text
    if (m_boss)
    {
        DirectX::XMFLOAT3 basePos = m_boss->GetVisualPosition();
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
        ImGui::DragFloat3("Boss Offset XYZ", &m_bossTrackingOffset.x, 0.1f);
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

    if (ImGui::CollapsingHeader("Window Shatter Debug"))
    {
        ImGui::Text("Active Shatter: %d", WindowShatterManager::Instance().GetActiveCount());
        if (ImGui::Button("Spawn Test Explosion"))
        {
            int sw, sh;
            GetScreenDimensions(sw, sh);
            WindowShatterManager::Instance().SpawnShatterExplosion(DirectX::XMFLOAT2(sw * 0.5f, sh * 0.5f), 10, 300.0f, 800.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) WindowShatterManager::Instance().Clear();
        if (ImGui::Button("Reset Intro")) { m_shatterSpawned = false; m_shatterTimer = 0.0f; }
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

    int winX, winY;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);
    int winW = targetWin->GetWidth();
    int winH = targetWin->GetHeight();

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) * 0.5f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    // Convert pixel to projection space [-1, 1]
    float l = ((float)winX / screenW) * 2.0f - 1.0f;
    float r = (((float)winX + winW) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - ((float)winY / screenH) * 2.0f;
    float b = 1.0f - (((float)winY + winH) / screenH) * 2.0f;

    targetCam->SetOffCenterProjection(l * halfWidth, r * halfWidth, b * halfHeight, t * halfHeight, nearZ, farZ);
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