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

// Definisi Konstanta Lokal (Karena sebelumnya hilang)
#define FIELD_OF_VIEW 60.0f
#define DEFERRED_INIT_TIME 0.2f
#define CACHE_REFRESH_INTERVAL 1.0f
#define PRIORITY_ENFORCE_INTERVAL 2.0f
#define PIXEL_TO_UNIT_RATIO 40.0f // Sesuaikan dengan settingan di WindowTrackingSystem

// =========================================================
// CONSTRUCTOR
// =========================================================
SceneGameBeyond::SceneGameBeyond()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;

    // 0. Setup Window System (PENTING: Init System Dulu!)
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(PIXEL_TO_UNIT_RATIO);
    m_windowSystem->SetFOV(FIELD_OF_VIEW);

    // Helper untuk hitung tinggi kamera dari System
    float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();

    // 1. Setup Main Camera
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0, 0, 0 });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 2. Setup Assets
    m_player = std::make_unique<Player>();
    m_player->SetInvertControls(true);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);

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

    // Tidak perlu loop m_trackedWindows manual, 
    // karena m_windowSystem destructor akan membersihkannya otomatis.
}

// =========================================================
// INITIALIZE SUB WINDOWS
// =========================================================
void SceneGameBeyond::InitializeSubWindows()
{
    // =====================================================
    // MENGGUNAKAN m_windowSystem UNTUK MEMBUAT WINDOW
    // =====================================================

    // 1. Player Tracking Window
    m_windowSystem->AddTrackedWindow(
        { "player", "Player View", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },
        [this]() { return m_player->GetPosition(); }
    );

    // 2. Boss Monitor 1 (Main Head)
    if (m_boss->HasPart("monitor1"))
    {
        m_windowSystem->AddTrackedWindow(
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
        m_windowSystem->AddTrackedWindow(
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
        m_windowSystem->AddTrackedWindow(
            { "monitor2", "Side Monitor L", 240, 210, 4, { 0.5f, 0.0f, -0.3f } },
            [this]() {
                auto pos = m_boss->GetMonitor2VisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    // 5. Monitor 3 (Side Right)
    if (m_boss->HasPart("monitor3"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor3", "Side Monitor R", 200, 200, 3, { 0.8f, 0.0f, 1.2f } },
            [this]() {
                auto pos = m_boss->GetMonitor3VisualPos();
                return XMFLOAT3{ pos.x, 0.0f, pos.z };
            }
        );
    }

    WindowManager::Instance().EnforceWindowPriorities();
    m_isWindowsInitialized = true;
}

// =========================================================
// UPDATE LOOP
// =========================================================
void SceneGameBeyond::Update(float elapsedTime)
{
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

    // Update Shatter Projections (Masih perlu helper dari system)
    float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();

    // Untuk Shatter, kita perlu sedikit trick karena dia bukan "TrackedWindow" standar
    // Kita pinjam helper logic dari system untuk menghitung proyeksi
    for (const auto& shatter : WindowShatterManager::Instance().GetShatters())
    {
        if (shatter->IsNativeWindow())
        {
            // PENTING: UpdateOffCenterProjection sekarang private di System.
            // Opsi 1: Jadikan public di System (RECOMMENDED).
            // Opsi 2: Scene meminta system melakukan kalkulasi (agak rumit).

            // Asumsi kamu sudah mengubah UpdateOffCenterProjection menjadi PUBLIC di WindowTrackingSystem.h
            // Jika belum, buka WindowTrackingSystem.h dan pindahkan UpdateOffCenterProjection ke bagian public.
            // m_windowSystem->UpdateOffCenterProjection(shatter->GetCamera(), shatter->GetWindow(), unifiedHeight);

            // *JIKA ERROR DI SINI*: Beritahu saya, kita akan buat helper public di System.
            // Untuk sementara saya komen agar compile jalan dulu, logika shatter mungkin agak aneh sementara.
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

    // 5. UPDATE WINDOW SYSTEM (Menggantikan UpdateAllTrackedWindows)
    m_windowSystem->Update(elapsedTime);

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
        // Menggunakan setter System, bukan variabel lokal
        static float speed = 100.0f;
        if (ImGui::DragFloat("Follow Speed", &speed, 0.1f, 0.1f, 50.0f)) {
            m_windowSystem->SetFollowSpeed(speed);
        }

        ImGui::Separator();

        // Akses windows via System getter
        const auto& windows = m_windowSystem->GetWindows();
        ImGui::Text("Tracked Windows: %d", (int)windows.size());

        for (auto& tracked : windows) // tracked adalah unique_ptr<TrackedWindow>
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
            // Gunakan Helper dari System
            m_windowSystem->WorldToScreenPos(worldPos, screenX, screenY);

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
// RENDER
// =========================================================
void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    if (!m_gameStarted && targetCam == m_mainCamera.get())
    {
        D3D11_VIEWPORT vp;
        UINT num = 1;
        dc->RSGetViewports(&num, &vp);
        m_primitive2D->Rect(0.0f, 0.0f, vp.Width, vp.Height, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        m_primitive2D->Render(dc);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    }

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
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    if (m_player) m_player->Render(modelRenderer);

    if (m_boss && m_gameStarted)
    {
        m_boss->Render(modelRenderer, camera);
    }

    if (m_blockManager && m_gameStarted)
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (block->IsActive())
                block->Render(modelRenderer, m_blockManager->globalBlockColor);
        }
    }

    modelRenderer->Render(rc);

    // Virtual Shatters (Overlay 2D)
    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    const auto& shatters = WindowShatterManager::Instance().GetShatters();
    for (const auto& shatter : shatters)
    {
        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            float screenX, screenY;
            // Gunakan Helper System
            m_windowSystem->WorldToScreenPos(worldPos, screenX, screenY);

            m_primitive2D->Rect(
                screenX, screenY,
                size.x, size.y,
                size.x * 0.5f, size.y * 0.5f,
                0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            );
        }
    }

    // Render Primitive yang baru ditumpuk
    m_primitive2D->Render(dc);

    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::TestAndWrite), 0);

    // Render Boss Text Override (Opsional)
    if (m_boss)
    {
        // ... Logika custom text lama kamu di sini ...
    }
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