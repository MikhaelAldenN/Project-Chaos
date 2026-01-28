#include <SDL3/SDL.h>
#include <cmath>
#include <windows.h>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGameBeyond.h"
#include "WindowManager.h"
#include "Framework.h"

#include <imgui.h>

using namespace DirectX;

// Definisi Konstanta Lokal
#define FIELD_OF_VIEW 60.0f
#define DEFERRED_INIT_TIME 0.2f
#define CACHE_REFRESH_INTERVAL 1.0f
#define PRIORITY_ENFORCE_INTERVAL 2.0f
#define PIXEL_TO_UNIT_RATIO 40.0f

// =========================================================
// CONSTRUCTOR
// =========================================================
SceneGameBeyond::SceneGameBeyond()
{
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    float screenW = 1280.0f;
    float screenH = 720.0f;

    // 0. Setup Window System
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(PIXEL_TO_UNIT_RATIO);
    m_windowSystem->SetFOV(FIELD_OF_VIEW);

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

    m_blockManager->shieldSettings.Enabled = true;
    m_blockManager->shieldSettings.MaxTetherDistance = 8.0f; // Jarak tameng agak jauh dikit
    m_blockManager->shootSettings.ProjectileSpeed = 20.0f;   // Kecepatan peluru

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
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    WindowShatterManager::Instance().Clear();
    CameraController::Instance().ClearCamera();
}

// =========================================================
// INITIALIZE SUB WINDOWS
// =========================================================
void SceneGameBeyond::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player || !m_boss) return;

    // 1. Player Tracking Window
    m_windowSystem->AddTrackedWindow(
        { "player", "Player View", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },
        [this]() -> DirectX::XMFLOAT3 {
            if (m_player) return m_player->GetPosition();
            return DirectX::XMFLOAT3(0, 0, 0);
        }
    );

    // 2. Boss Monitor 1 (Main Head)
    if (m_boss->HasPart("monitor1"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor1", "Boss Monitor", 340, 340, 0, { -0.3f, 0.0f, 2.1f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitorVisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 3. CPU (Body)
    if (m_boss->HasPart("cpu"))
    {
        m_windowSystem->AddTrackedWindow(
            { "cpu", "System Unit", 186, 370, 0, { -8.2f, 0.0f, 4.0f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetCPUVisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 4. Monitor 2 (Side Left)
    if (m_boss->HasPart("monitor2"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor2", "Side Monitor L", 240, 210, 4, { 0.5f, 0.0f, -0.3f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitor2VisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 5. Monitor 3 (Side Right)
    if (m_boss->HasPart("monitor3"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor3", "Side Monitor R", 200, 200, 3, { 0.8f, 0.0f, 1.2f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitor3VisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
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
                    WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 8);
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
        return;
    }

    // 3. Update Managers
    WindowShatterManager::Instance().Update(elapsedTime);

    // 4. Game Logic
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    DirectX::XMFLOAT3 mousePos = GetMouseOnGround(activeCam);

    if (m_player)
    {
        m_player->Update(elapsedTime, activeCam);

        DirectX::XMFLOAT3 pos = m_player->GetPosition();
        if (pos.y < 0.0f)
        {
            pos.y = 0.0f;
            m_player->SetPosition(pos.x, pos.y, pos.z);

            // Opsional: Jika player punya variabel velocity/gravity, 
            // sebaiknya di-reset ke 0 disini agar tidak menumpuk, 
            // tapi SetPosition saja biasanya cukup untuk visual.
        }

        if (m_subCamera) m_subCamera->LookAt(m_player->GetPosition());
    }

    // --- INTEGRASI SHOOT & SHIELD ---
    if (m_blockManager && m_player)
    {
        // Deteksi Input (Menggunakan WinAPI GetKeyState agar responsif untuk hold)
        // VK_LSHIFT = Tameng, VK_SPACE = Tembak
        bool isShielding = (GetKeyState(VK_LSHIFT) & 0x8000);
        bool isShooting = (GetKeyState(VK_SPACE) & 0x8000);

        DirectX::XMFLOAT3 playerPos = m_player->GetPosition();

        // Panggil fungsi logika di BlockManager
        m_blockManager->UpdateShieldLogic(isShielding, mousePos, playerPos, elapsedTime);
        m_blockManager->UpdateShootLogic(isShooting, mousePos, playerPos, elapsedTime);

        // Update biasa
        m_blockManager->Update(elapsedTime, activeCam, m_player.get());

        // Anti-fall untuk blocks (Kode sebelumnya)
        auto& blocks = m_blockManager->GetBlocks();
        for (auto& block : blocks) {
            if (block && block->IsActive()) {
                auto movement = block->GetMovement();
                DirectX::XMFLOAT3 bPos = movement->GetPosition();

                // JIKA JATUH ATAU BERADA DI BAWAH 0
                if (bPos.y < 0.0f)
                {
                    // 1. Reset Posisi ke 0
                    movement->SetPosition({ bPos.x, 0.0f, bPos.z });

                    // 2. HENTIKAN KECEPATAN JATUH (PENTING BIAR GAK JITTER)
                    DirectX::XMFLOAT3 vel = movement->GetVelocity();
                    if (vel.y < 0) {
                        // Nol-kan kecepatan Y, tapi biarkan X dan Z (biar tetap bisa geser)
                        movement->SetVelocity({ vel.x, 0.0f, vel.z });
                    }
                }
            }
        }

        // Debug Spawn manual (tombol R)
        if (Input::Instance().GetKeyboard().IsTriggered('R'))
            m_blockManager->SpawnAllyBlock(m_player.get());
    }
    // --------------------------------

    if (m_boss) m_boss->Update(elapsedTime);


    // 5. Update Window System
    if (m_windowSystem)
    {
        m_windowSystem->Update(elapsedTime);
    }

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

static void DrawTransformControl(const char* label, DirectX::XMFLOAT3* pos)
{
    if (ImGui::TreeNode(label))
    {
        if (pos) ImGui::DragFloat3("Position", &pos->x, 0.1f);
        ImGui::TreePop();
    }
}

// =========================================================
// DRAW GUI - SCENE BEYOND (VERSI FIX)
// =========================================================
void SceneGameBeyond::DrawGUI()
{
    // Setting Window (Posisi & Ukuran awal)
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("BEYOND INSPECTOR", nullptr))
    {
        if (ImGui::BeginTabBar("BeyondTabs"))
        {
            // --- TAB 1: GENERAL & SHATTER ---
            if (ImGui::BeginTabItem("General"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "GAME STATE");
                ImGui::Checkbox("Game Started", &m_gameStarted);
                ImGui::Checkbox("Shatter Triggered", &m_shatterTriggered);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WINDOW SHATTER FX");

                // Tombol Manual Ledakan Kaca
                if (ImGui::Button("TRIGGER EXPLOSION", ImVec2(-1, 40)))
                {
                    if (m_player) {
                        auto pPos = m_player->GetPosition();
                        WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 12);
                        m_shatterTriggered = true;
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Clear All Shards")) {
                    WindowShatterManager::Instance().Clear();
                }

                // Info jumlah pecahan
                auto& shards = WindowShatterManager::Instance().GetShatters();
                ImGui::Text("Active Shards: %d", (int)shards.size());

                ImGui::EndTabItem();
            }

            // --- TAB 2: OBJECTS ---
            if (ImGui::BeginTabItem("Objects"))
            {
                ImGui::Spacing();
                ImGui::Text("Scene Objects:");
                ImGui::Separator();

                // 1. PLAYER
                if (m_player)
                {
                    // Kita ambil posisi saja karena GetRotation tidak ada
                    XMFLOAT3 pos = m_player->GetPosition();

                    ImGui::PushID("Ply");
                    if (ImGui::TreeNode("Player"))
                    {
                        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                            m_player->SetPosition(pos.x, pos.y, pos.z);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                // 2. BOSS
                if (m_boss)
                {
                    ImGui::Spacing();
                    if (ImGui::TreeNode("Boss Controller"))
                    {
                        ImGui::TextDisabled("Boss Logic Active");
                        ImGui::TreePop();
                    }
                }

                ImGui::EndTabItem();
            }

            // --- TAB 3: WINDOW SYSTEM ---
            if (ImGui::BeginTabItem("Window System"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TRACKED WINDOWS");
                ImGui::Separator();

                if (m_windowSystem)
                {
                    ImGui::Text("Unified FOV: %.1f", FIELD_OF_VIEW);
                    ImGui::TextDisabled("Multi-window system active.");
                }
                else
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Window System is NULL");
                }

                ImGui::EndTabItem();
            }

            // --- TAB 4: CAMERA ---
            if (ImGui::BeginTabItem("Camera"))
            {
                if (m_mainCamera)
                {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MAIN CAMERA");

                    XMFLOAT3 pos = m_mainCamera->GetPosition();
                    if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                        m_mainCamera->SetPosition(pos);
                    }

                    // HAPUS BAGIAN LOOKAT KARENA ERROR
                    // Kita cuma tampilkan tombol reset
                    if (ImGui::Button("Reset Camera Pos")) {
                        m_mainCamera->SetPosition(0, 5, 0);
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
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
        if (!shatter) continue;

        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            float screenX, screenY;
            if (m_windowSystem)
            {
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
    }

    m_primitive2D->Render(dc);

    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::TestAndWrite), 0);
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

// GANTI FUNGSI INI DI SceneGameBeyond.cpp
DirectX::XMFLOAT3 SceneGameBeyond::GetMouseOnGround(Camera* camera)
{
    // 1. Ambil Posisi Mouse Global (Koordinat Layar Monitor)
    POINT pt;
    if (!GetCursorPos(&pt)) return { 0,0,0 }; // WinAPI function

    // 2. Ambil Ukuran Layar Monitor Utama (Resolusi Asli)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 3. Hitung Titik Tengah Layar (Center of World 0,0,0)
    float centerX = screenW / 2.0f;
    float centerY = screenH / 2.0f;

    // 4. Hitung Jarak Mouse dari Tengah Layar (dalam Pixel)
    float dx = (float)pt.x - centerX;
    float dy = (float)pt.y - centerY;

    // 5. Konversi Pixel ke World Unit
    // Kita gunakan Ratio yang sama dengan WindowTrackingSystem (40.0f)
    // Rumus: Jarak Pixel / Ratio = Jarak World

    float worldX = dx / PIXEL_TO_UNIT_RATIO;

    // Note: Screen Y positif ke bawah, tapi World Z positif ke atas (tergantung kamera).
    // Biasanya untuk top-down view desktop: Y layar turun = Z world mundur.
    float worldZ = -dy / PIXEL_TO_UNIT_RATIO;

    return DirectX::XMFLOAT3(worldX, 0.0f, worldZ);
}