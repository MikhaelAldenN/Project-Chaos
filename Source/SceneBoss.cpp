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

// =========================================================
// CONSTRUCTOR
// =========================================================
SceneBoss::SceneBoss()
{
    // Disable ImGui viewports for stability in this scene
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    float screenW = 1920.0f;
    float screenH = 1080.0f;

    // 1. Initialize Window Tracking System
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(PIXEL_TO_UNIT_RATIO);
    m_windowSystem->SetFOV(FIELD_OF_VIEW);

    float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();

    // 2. Setup Main Camera
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0.0f, 0.0f, 0.0f });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 3. Setup Player
    m_player = std::make_unique<Player>();
    m_player->SetInvertControls(false);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);

    // 4. Setup Primitive Renderer
    auto device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);
    m_primitive3D = std::make_unique<PrimitiveRenderer>(device);

    // Immediately trigger Windowkill mode
    InitializeSubWindows();
}

// =========================================================
// DESTRUCTOR
// =========================================================
SceneBoss::~SceneBoss()
{
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    CameraController::Instance().ClearCamera();
}

// =========================================================
// INITIALIZATION
// =========================================================
void SceneBoss::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player) return;

    // Add Player Tracking Window
    m_windowSystem->AddTrackedWindow(
        { "player", "Player View", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },

        // Lambda 1: Target Position (Follows Player)
        [this]() -> DirectX::XMFLOAT3 {
            if (!m_player) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
            auto pPos = m_player->GetPosition();
            return DirectX::XMFLOAT3(pPos.x, 0.0f, pPos.z);
        },

        // Lambda 2: Target Size (Fixed Padding)
        [this]() -> DirectX::XMFLOAT2 {
            float safeMarginPixel = 150.0f;
            float extraPadding = 50.0f;
            float size = (safeMarginPixel * 2.0f) + extraPadding;
            return DirectX::XMFLOAT2(size, size);
        }
    );

    GameWindow* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow())
    {
        SDL_HideWindow(mainWindow->GetSDLWindow());
    }

    WindowManager::Instance().EnforceWindowPriorities();
}

// =========================================================
// CORE UPDATE
// =========================================================
void SceneBoss::Update(float elapsedTime)
{
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // 1. Update Player & Enforce Screen Boundaries
    if (m_player)
    {
        m_player->Update(elapsedTime, activeCam);

        DirectX::XMFLOAT3 pos = m_player->GetPosition();

        // Clamp player position to arena limits cleanly
        pos.x = std::clamp(pos.x, -m_screenLimitX, m_screenLimitX);
        pos.z = std::clamp(pos.z, -m_screenLimitZ, m_screenLimitZ);
        pos.y = max(pos.y, 0.0f);

        m_player->SetPosition(pos.x, pos.y, pos.z);
    }

    if (m_player && m_mainCamera)
    {
        // 1. Ambil resolusi monitor asli dari sistem Mouse
        float screenW = (float)Input::Instance().GetMouse().GetScreenWidth();
        float screenH = (float)Input::Instance().GetMouse().GetScreenHeight();

        // 2. Buat matriks proyeksi FULL SCREEN standar yang belum di-crop (FOV 60)
        DirectX::XMMATRIX standardProj = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(60.0f),
            screenW / screenH,
            0.1f,
            1000.0f
        );

        DirectX::XMFLOAT4X4 cleanProjFloat;
        DirectX::XMStoreFloat4x4(&cleanProjFloat, standardProj);

        // 3. Gunakan matriks standar ini untuk mencari posisi mouse di 3D!
        DirectX::XMFLOAT3 targetPos = Input::Instance().GetMouse().GetWorldPosition(
            m_mainCamera->GetView(),
            cleanProjFloat
        );

        m_player->RotateModelToPoint(targetPos);
        m_player->Update(elapsedTime, m_mainCamera.get());
    }

    // 2. Update Window Tracking
    if (m_windowSystem)
    {
        m_windowSystem->Update(elapsedTime);
    }

    CameraController::Instance().Update(elapsedTime);
}

// =========================================================
// CORE RENDER
// =========================================================
void SceneBoss::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // Reset Render States for safety
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);


    // Draw floor grid
    if (m_primitive3D)
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

    // Draw Player if within camera bounds
    if (m_player)
    {
        XMFLOAT3 pPos = m_player->GetPosition();
        if (camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f))
        {
            m_player->Render(modelRenderer);
        }
    }

    // Flush model rendering queue
    modelRenderer->Render(rc);
}

// =========================================================
// DEBUG GUI
// =========================================================
void SceneBoss::DrawGUI()
{
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("BEYOND INSPECTOR", nullptr))
    {
        if (ImGui::BeginTabBar("BeyondTabs"))
        {
            if (ImGui::BeginTabItem("General")) { DrawTabGeneral(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Camera")) { DrawTabCamera();  ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void SceneBoss::DrawTabGeneral()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PLAYER BOUNDARY (1920x1080)");
    ImGui::TextDisabled("Ratio: 1 Unit = 40 Pixels");

    ImGui::Indent();
    ImGui::DragFloat("Limit X (Width)", &m_screenLimitX, 0.1f, 10.0f, 50.0f);
    ImGui::DragFloat("Limit Z (Height)", &m_screenLimitZ, 0.1f, 5.0f, 30.0f);
    ImGui::Unindent();
    ImGui::Separator();

    if (m_player)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "PLAYER");
        XMFLOAT3 pos = m_player->GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
            m_player->SetPosition(pos.x, pos.y, pos.z);
        }
    }
}

void SceneBoss::DrawTabCamera()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MAIN CAMERA DEBUG");
    ImGui::Separator();

    if (m_mainCamera)
    {
        XMFLOAT3 pos = m_mainCamera->GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
            m_mainCamera->SetPosition(pos);
        }

        ImGui::Spacing();
        if (ImGui::Button("Reset Camera Pos")) {
            float unifiedHeight = m_windowSystem ? m_windowSystem->GetUnifiedCameraHeight() : 5.0f;
            m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
        }
    }
}

// =========================================================
// WINDOW EVENTS
// =========================================================
void SceneBoss::OnResize(int width, int height)
{
    if (m_mainCamera)
    {
        m_mainCamera->SetAspectRatio(static_cast<float>(width) / max(1, height));
    }
}