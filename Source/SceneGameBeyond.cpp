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

    // 2. Setup Assets
    player = std::make_unique<Player>();
    player->SetInvertControls(true);

    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player.get());
    blockManager->ClearBlocks();
    blockManager->ActivateFormationMode();

    int allyCount = 20;
    for (int i = 0; i < allyCount; ++i) blockManager->SpawnAllyBlock(player.get());

    // ----------------------------------------------------------------
    // [FIX] SETUP BOSS SESUAI GAMBAR DARI USER
    // ----------------------------------------------------------------
    boss = std::make_unique<Boss>();
    // Position: 0.0, 0.6, 6.5
    boss->SetPosition(0.0f, 0.6f, 6.5f);
    // Rotation: 65, 0, 0
    boss->SetRotation(65.0f, 0.0f, 0.0f);
    // Scale: 10, 10, 10
    boss->SetScale(10.0f, 10.0f, 10.0f);
    // Floating: Enable, Speed 0.5, Intensity 0.2, Axis (0,0,1 / Z-Axis)
    boss->SetFloatingConfig(true, 2.0f, 0.2f, { 0.0f, 0.0f, 1.0f });

    // 3. Sub Camera
    subCamera = std::make_shared<Camera>();
    subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    subCamera->SetPosition(5, 5, 5);
    subCamera->LookAt({ 0, 0, 0 });

    // Reset Pointers
    trackingWindow = nullptr;
    lensWindow = nullptr;
    bossWindow = nullptr;
}

SceneGameBeyond::~SceneGameBeyond()
{
    WindowShatterManager::Instance().Clear();
    CameraController::Instance().ClearCamera();
    WindowManager::Instance().DestroyWindow(trackingWindow);
    WindowManager::Instance().DestroyWindow(lensWindow);
    WindowManager::Instance().DestroyWindow(bossWindow);
    additionalCameras.clear();
}

void SceneGameBeyond::InitializeSubWindows()
{
    // ---------------------------------------------
    // 1. Player Tracking Window
    // ---------------------------------------------
    trackingWindow = WindowManager::Instance().CreateGameWindow("Player View", 300, 300);
    trackingWindow->SetPriority(1);
    trackingWindow->SetDraggable(false);
    trackingCamera = std::make_shared<Camera>();
    trackingWindow->SetCamera(trackingCamera.get());

    // ---------------------------------------------
    // 2. Lens Window
    // ---------------------------------------------
    lensWindow = WindowManager::Instance().CreateGameWindow("Lens View", 300, 300);
    lensWindow->SetPriority(0);
    lensCamera = std::make_shared<Camera>();
    lensCamera->SetRotation(90.0f, 0.0f, 0.0f);
    lensWindow->SetCamera(lensCamera.get());

    // ---------------------------------------------
    // 3. BOSS TRACKING WINDOW
    // ---------------------------------------------
    bossWindow = WindowManager::Instance().CreateGameWindow("Boss Monitor", 370, 370);
    bossWindow->SetPriority(2); // Di bawah player dikit (semakin kecil semakin atas, 0=top)
	bossWindow->SetDraggable(false);
    bossCamera = std::make_shared<Camera>();
    bossWindow->SetCamera(bossCamera.get());

    // --- INITIAL POSITIONING ---
    // Agar window tidak "terbang" saat start, kita set posisi awalnya manual
    auto centerScreen = [](float& x, float& y, DirectX::XMFLOAT3 pos, GameWindow* w)
        {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            float tx = (sw / 2.0f) + (pos.x * PIXEL_TO_UNIT_RATIO);
            float ty = (sh / 2.0f) - (pos.z * PIXEL_TO_UNIT_RATIO);
            x = tx - (w->GetWidth() / 2.0f);
            y = ty - (w->GetHeight() / 2.0f);
            SDL_SetWindowPosition(w->GetSDLWindow(), (int)x, (int)y);
        };

    if (player && trackingWindow) centerScreen(m_playerWinX, m_playerWinY, player->GetPosition(), trackingWindow);

    // Init Boss Window Pos
    if (boss && bossWindow) centerScreen(m_bossWinX, m_bossWinY, boss->GetPosition(), bossWindow);

    // Hide Main Window
    if (auto mainWin = Framework::Instance()->GetMainWindow()) mainWin->SetVisible(false);

    WindowManager::Instance().EnforceWindowPriorities();
    isWindowsInitialized = true;
}

// --------------------------------------------------------------------------
// [BARU] FUNGSI GENERIK TRACKING (CORE LOGIC)
// Fungsi ini bisa dipakai untuk Player, Boss, atau Object apapun!
// --------------------------------------------------------------------------
void SceneGameBeyond::UpdateWindowTracking(float dt, GameWindow* win, Camera* cam, const DirectX::XMFLOAT3& targetPos, float& currentX, float& currentY)
{
    if (!win || !cam) return;

    // 1. Hitung Target Posisi di Layar Monitor
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Konversi World Position -> Screen Pixel Coordinate
    // Center Screen = World (0,0)
    float targetScreenX = (screenW / 2.0f) + (targetPos.x * PIXEL_TO_UNIT_RATIO);
    float targetScreenY = (screenH / 2.0f) - (targetPos.z * PIXEL_TO_UNIT_RATIO); // Z terbalik di layar (Atas = -Y)

    // Pusatkan Window (Anchor di tengah window)
    float destX = targetScreenX - (win->GetWidth() / 2.0f);
    float destY = targetScreenY - (win->GetHeight() / 2.0f);

    // 2. Smooth Damping (Lerp)
    float t = m_windowFollowSpeed * dt;
    if (t > 1.0f) t = 1.0f;

    currentX += (destX - currentX) * t;
    currentY += (destY - currentY) * t;

    // 3. Terapkan Posisi Window
    SDL_SetWindowPosition(win->GetSDLWindow(), (int)currentX, (int)currentY);

    // 4. Update Kamera Off-Center Projection
    // Agar prespektif di dalam window terlihat "menyambung" dengan dunia
    float centerY = screenH / 2.0f;
    float camHeight = (centerY / (PIXEL_TO_UNIT_RATIO * tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f)));
    UpdateOffCenterProjection(cam, win, camHeight);
}

void SceneGameBeyond::Update(float elapsedTime)
{
    // ==========================================
    // [INTRO EFFECT] Window Shatter Explosion
    // ==========================================
    if (!m_shatterSpawned)
    {
        m_shatterTimer += elapsedTime;

        // Tunggu sebentar sebelum spawn (agar player siap)
        if (m_shatterTimer >= m_shatterDelay)
        {
            // Hitung posisi spawn di screen space (di bawah player)
            if (player)
            {
                // Konversi posisi player (world space) ke screen space
                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int screenH = GetSystemMetrics(SM_CYSCREEN);

                DirectX::XMFLOAT3 playerPos = player->GetPosition();

                float screenX = (screenW / 2.0f) + (playerPos.x * PIXEL_TO_UNIT_RATIO);
                float screenY = (screenH / 2.0f) - (playerPos.z * PIXEL_TO_UNIT_RATIO);

                // Offset ke bawah player sedikit (misal 100 pixel)
                screenY += 100.0f;

                // Spawn shatter explosion!
                WindowShatterManager::Instance().SpawnShatterExplosion(
                    DirectX::XMFLOAT2(screenX, screenY),
                    8,      // 10 windows
                    4000.0f,  // Min speed
                    4000.0f   // Max speed
                );

                m_shatterSpawned = true;
            }
        }
    }

    WindowShatterManager::Instance().Update(elapsedTime);

    if (!isWindowsInitialized)
    {
        startupTimer += elapsedTime;
        if (startupTimer > DEFERRED_INIT_TIME) InitializeSubWindows();
        else return;
    }

    // ==========================================
    // 1. UPDATE LOGIC GAME (Animasi, Move, AI)
    // ==========================================
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    if (player)
    {
        player->Update(elapsedTime, activeCam);
        if (subCamera) subCamera->LookAt(player->GetPosition());
    }

    if (boss)
    {
        // HANYA Update animasi/logika boss disini.
        // JANGAN panggil UpdateWindowTracking disini!
        boss->Update(elapsedTime);
    }

    if (blockManager)
    {
        blockManager->Update(elapsedTime, activeCam, player.get());
        if (Input::Instance().GetKeyboard().IsTriggered('R')) blockManager->SpawnAllyBlock(player.get());
    }

    // ==========================================
    // 2. UPDATE WINDOW LOGICS (Fisik Window)
    // ==========================================
    if (isWindowsInitialized)
    {
        // --- A. Player Window ---
        if (player)
        {
            UpdateWindowTracking(elapsedTime, trackingWindow, trackingCamera.get(),
                player->GetPosition(), m_playerWinX, m_playerWinY);
        }

        // --- B. Boss Window (FIX JITTER) ---
        if (boss)
        {
            // 1. Ambil posisi visual (animasi floating)
            DirectX::XMFLOAT3 targetPos = boss->GetVisualPosition();

            // 2. Terapkan Offset manual (biar bisa geser-geser via Editor)
            targetPos.x += m_bossTrackingOffset.x;
            targetPos.y += m_bossTrackingOffset.y;
            targetPos.z += m_bossTrackingOffset.z;

            // 3. Panggil Tracking SEKALI SAJA disini
            UpdateWindowTracking(elapsedTime, bossWindow, bossCamera.get(),
                targetPos, m_bossWinX, m_bossWinY);
        }

        UpdateLensLogic();
        WindowManager::Instance().EnforceWindowPriorities();
    }

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);
}

// -------------------------------------------------------------------------
// RENDER & GUI
// -------------------------------------------------------------------------

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
    auto primRenderer = Graphics::Instance().GetPrimitiveRenderer();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();

    // 1. Render Grid
    primRenderer->DrawGrid(20, 1);
    primRenderer->Render(dc, camera->GetView(), camera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // 2. Setup Culling
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };
    DirectX::XMFLOAT3 camPos = camera->GetPosition();
    float cullingRadius = 40.0f;

    auto IsVisible = [&](const DirectX::XMFLOAT3& objPos) -> bool {
        float dx = camPos.x - objPos.x;
        float dz = camPos.z - objPos.z;
        return (dx * dx + dz * dz) < (cullingRadius * cullingRadius);
        };

    // 3. Render Player
    if (player) player->Render(modelRenderer);

    // 4. Render Blocks
    if (blockManager)
    {
        for (const auto& block : blockManager->GetBlocks())
        {
            if (!block->IsActive()) continue;
            if (!IsVisible(block->GetMovement()->GetPosition())) continue;
            block->Render(modelRenderer, blockManager->globalBlockColor);
        }
    }

    // 5. Render Boss
    if (boss) boss->Render(modelRenderer);

    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");


    modelRenderer->Render(rc);

    if (boss)
    {
        // 1. Hitung Posisi Akhir (Posisi Boss + Offset Editor)
        DirectX::XMFLOAT3 basePos = boss->GetVisualPosition();
        DirectX::XMFLOAT3 finalPos = {
            basePos.x + m_textOffset.x,
            basePos.y + m_textOffset.y,
            basePos.z + m_textOffset.z
        };

        // 2. Logika Rotasi
        DirectX::XMFLOAT3 rotationInRadians;

        rotationInRadians.x = DirectX::XMConvertToRadians(m_textRotation.x);
        rotationInRadians.y = DirectX::XMConvertToRadians(m_textRotation.y);
        rotationInRadians.z = DirectX::XMConvertToRadians(m_textRotation.z);

        // 3. Gambar!
        // Pastikan pakai m_textLabel (char array) bukan string literal
        text->Draw3D(
            m_textLabel,
            camera,
            finalPos,
            m_textScale,
            rotationInRadians,
            m_textColor
        );
    }

}

void SceneGameBeyond::DrawGUI()
{
    CameraController::Instance().DrawDebugGUI();

    ImGui::Begin("Scene Info");
    ImGui::Text("Controls: WASD (Move), N (New Window)");

    if (player) player->DrawDebugGUI();
    ImGui::Separator();
    if (boss) boss->DrawDebugGUI();

    // [BARU] Editor untuk Window Tracking Offset
    if (ImGui::CollapsingHeader("Window Tracking Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Window Follow Speed", &m_windowFollowSpeed, 0.1f, 0.1f, 50.0f);

        ImGui::Text("Boss Window Offset (World Space):");
        ImGui::DragFloat3("Offset XYZ", &m_bossTrackingOffset.x, 0.1f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Gunakan Y untuk naik/turunkan window");
    }

    ImGui::Separator();

    // --- TEXT EDITOR WINDOW ---
    if (ImGui::CollapsingHeader("3D Text Editor", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 1. Edit Isi Teks
        ImGui::InputText("Label Content", m_textLabel, sizeof(m_textLabel));

        // 2. Edit Offset (Relatif terhadap Boss)
        ImGui::DragFloat3("Offset Position", &m_textOffset.x, 0.1f);

        // 3. Edit Rotasi
        // Gunakan Slider/Drag. Ingat ini dalam Degree (Derajat)
        ImGui::DragFloat3("Rotation (Pitch/Yaw/Roll)", &m_textRotation.x, 1.0f, -360.0f, 360.0f);

        // 4. Edit Scale (Penting! Font 3D itu sensitif ukurannya)
        ImGui::DragFloat("Scale", &m_textScale, 0.001f, 0.001f, 1.0f, "%.4f");

        // 5. Edit Warna
        ImGui::ColorEdit4("Text Color", &m_textColor.x);

        // 6. Tombol Reset (Biar gampang kalau teksnya hilang entah kemana)
        if (ImGui::Button("Reset Text Config"))
        {
            sprintf_s(m_textLabel, ">C:\\_"); // Reset teks juga
            m_textOffset = { -2.3f, 3.8f, 0.0f };
            m_textRotation = { 68.0f, 0.0f, 0.0f };
            m_textScale = 0.011f;
            m_textColor = { 237.0f / 255.0f, 192.0f / 255.0f, 17.0f / 255.0f, 1.0f };
        }
    }

    ImGui::DragFloat("Window Follow Speed", &m_windowFollowSpeed, 0.1f, 0.1f, 50.0f);

    if (ImGui::CollapsingHeader("Window Shatter Debug"))
    {
        ImGui::Text("Active Shatter: %d", WindowShatterManager::Instance().GetActiveCount());

        if (ImGui::Button("Spawn Test Explosion (Center)"))
        {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            WindowShatterManager::Instance().SpawnShatterExplosion(
                DirectX::XMFLOAT2(sw / 2.0f, sh / 2.0f),
                10, 300.0f, 800.0f
            );
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear All Shatter"))
        {
            WindowShatterManager::Instance().Clear();
        }

        // Option untuk re-trigger intro
        if (ImGui::Button("Reset Intro (Re-spawn Shatter)"))
        {
            m_shatterSpawned = false;
            m_shatterTimer = 0.0f;
        }

        // Tweak parameters
        ImGui::DragFloat("Shatter Delay", &m_shatterDelay, 0.1f, 0.0f, 5.0f);
    }

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

// -------------------------------------------------------------------------
// HELPER FUNCTIONS (Ini yang sering ketinggalan!)
// -------------------------------------------------------------------------

float SceneGameBeyond::GetUnifiedCameraHeight() const
{
    int sysScreenH = GetSystemMetrics(SM_CYSCREEN);
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f);
    return (sysScreenH / 2.0f) / (PIXEL_TO_UNIT_RATIO * halfFovTan);
}

void SceneGameBeyond::UpdateLensLogic()
{
    if (!lensWindow || !lensCamera) return;
    UpdateOffCenterProjection(lensCamera.get(), lensWindow, GetUnifiedCameraHeight());
}

void SceneGameBeyond::UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight)
{
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    targetCam->SetPosition(0.0f, camHeight, 0.0f);
    targetCam->LookAt({ 0.0f, 0.0f, 0.0f });

    int winX, winY;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);
    int winW = targetWin->GetWidth();
    int winH = targetWin->GetHeight();

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(FIELD_OF_VIEW) / 2.0f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    float l = ((float)winX / screenW) * 2.0f - 1.0f;
    float r = (((float)winX + winW) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - ((float)winY / screenH) * 2.0f;
    float b = 1.0f - (((float)winY + winH) / screenH) * 2.0f;

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