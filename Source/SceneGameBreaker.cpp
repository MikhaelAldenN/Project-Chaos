#include <imgui.h>
#include <SDL3/SDL.h>
#include "Framework.h"
#include "SceneGameBreaker.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include "System/Input.h"

using namespace DirectX;

SceneGameBreaker::SceneGameBreaker()
{
    float screenW = 1280.0f;
    float screenH = 720.0f;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // 1. Setup Main Camera (Shared Ptr)
    mainCamera = std::make_shared<Camera>();
    mainCamera->SetPerspectiveFov(XMConvertToRadians(initialFOV), screenW / screenH, cameraNearZ, cameraFarZ);

    // [PENTING] Set ke Controller menggunakan Shared Ptr
    auto& camCtrl = CameraController::Instance();
    camCtrl.SetActiveCamera(mainCamera);

    // Setup Posisi Awal
    XMFLOAT3 startPos = cameraPosition;
    startPos.z = -2.0f;

    // Init Camera State
    mainCamera->SetPosition(startPos);
    mainCamera->LookAt(cameraTarget);

    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
    camCtrl.SetFixedSetting(startPos);
    camCtrl.SetTarget(cameraTarget);

    // --------------------------------------------------------------------
        // SETUP SEQUENCE DATA (A -> B -> C)
        // --------------------------------------------------------------------
        // Kita memanfaatkan mainCamera untuk menghitung rotasi Quaternion/Euler yang benar
        // lalu kita simpan datanya ke struct m_poseA/B/C.

        // --- POS A (Start) ---
    m_poseA.Position = startPos;
    m_poseA.TargetLookAt = cameraTarget;
    // Trik: Set main camera ke posisi ini sebentar untuk ambil rotasinya
    mainCamera->SetPosition(startPos);
    mainCamera->LookAt(cameraTarget);
    m_poseA.Rotation = mainCamera->GetRotation();

    // --- POS C (End) ---
    XMFLOAT3 posC = cameraPosition;
    posC.x = -8.0f; posC.y = 12.0f; posC.z = -6.0f;
    m_poseC.Position = posC;
    m_poseC.TargetLookAt = cameraTarget;

    mainCamera->SetPosition(posC);
    mainCamera->LookAt(cameraTarget);
    // Reset Roll biar lurus
    XMFLOAT3 rotC = mainCamera->GetRotation();
    rotC.z = 0.0f;
    mainCamera->SetRotation(rotC);
    m_poseC.Rotation = rotC;

    // --- POS B (Mid) ---
    XMFLOAT3 posB;
    posB.x = (startPos.x + posC.x) * 0.5f;
    posB.y = (startPos.y + posC.y) * 0.5f + 2.0f;
    posB.z = (startPos.z + posC.z) * 0.5f;

    m_poseB.Position = posB;
    m_poseB.TargetLookAt = cameraTarget;

    mainCamera->SetPosition(posB);
    mainCamera->LookAt(cameraTarget);
    m_poseB.Rotation = mainCamera->GetRotation();

    // Reset Main Camera ke posisi Awal (A) sebelum render frame pertama
    mainCamera->SetPosition(startPos);
    mainCamera->LookAt(cameraTarget);

    // --------------------------------------------------------
    // Initialize Assets
    // --------------------------------------------------------
    m_backgroundSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), backgroundPath);

    ball = new Ball();
    paddle = new Paddle();
    paddle->SetAIEnabled(false);
    player = new Player();
    player->SetInputEnabled(false);
    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player);

    vignetteShader = std::make_unique<VignetteShader>(Graphics::Instance().GetDevice());
    CreateRenderTarget();
}

SceneGameBreaker::~SceneGameBreaker()
{
    // [UPDATE] Cleanup lebih bersih
    CameraController::Instance().ClearCamera();

    // Tidak perlu delete mainCamera, m_poseA, dll. (Automatic memory management)

    if (player) delete player;
    if (paddle) delete paddle;
    if (ball) delete ball;
}

void SceneGameBreaker::Update(float elapsedTime)
{
    m_globalTime += elapsedTime;

    if (m_globalTime > 1000.0f) m_globalTime -= 1000.0f;

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // --------------------------------------------------------
    // HANDLE ANIMATION (TRANSITION)
    // --------------------------------------------------------
    //if (m_isAnimating)
    //{
    //    UpdateAnimation(elapsedTime);
    //}

    // --------------------------------------------------------
    // Update Paddle & AI
    // --------------------------------------------------------
   
    if (paddle)
    {
        if (ball) paddle->UpdateAI(elapsedTime, ball, blockManager.get());
        paddle->Update(elapsedTime, activeCam);
    }

    // --------------------------------------------------------
    // Update Ball Logic
    // --------------------------------------------------------
    if (ball)
    {
        if (paddle && !ball->IsActive())
        {
            XMFLOAT3 padPos = paddle->GetPosition();
            padPos.z += ballSpawnZOffset;
            padPos.y = 0.0f;
            ball->GetMovement()->SetPosition(padPos);
        }
    
        ball->Update(elapsedTime, activeCam);
    
        // Paddle Collision
        if (paddle && ball->IsActive())
        {
            paddle->CheckCollision(ball);
        }
    
    }

    // --------------------------------------------------------
    // Update Block Manager
    // --------------------------------------------------------
    if (blockManager)
    {
        blockManager->Update(elapsedTime, activeCam);
        if (ball && ball->IsActive()) blockManager->CheckCollision(ball);
    }

    // --------------------------------------------------------
    // Update Player
    // --------------------------------------------------------
    if (player) player->Update(elapsedTime, activeCam);

    CameraController::Instance().Update(elapsedTime);
    UpdateGameTriggers(elapsedTime);
}

void SceneGameBreaker::UpdateGameTriggers(float elapsedTime)
{
    if (m_isAnimating || m_hasTriggered) return;

    // Check if blocks are low enough to trigger Breakout
    bool triggerCondition = (GetKeyState('T') & 0x8000) // For debug only delete later 
        || (blockManager && blockManager->GetActiveBlockCount() <= triggerBlockCount);

    if (triggerCondition)
    {
        m_isAnimating = true;
        m_animTimer = 0.0f;
    }
}

void SceneGameBreaker::UpdateAnimation(float elapsedTime)
{
    m_animTimer += elapsedTime;
    if (m_animTimer > 100.0f) m_animTimer -= 100.0f;
    float t = (std::min)(m_animTimer / animDuration, 1.0f);
    float smoothT = t * t * (3.0f - 2.0f * t);

    // Camera Rotation 
    float currentOffset = animCameraRotationTotal * smoothT;
    CameraController::Instance().SetFixedYawOffset(currentOffset);

    // Sprite Rotation
    m_bgRotation = animTargetBgRotation * smoothT;
    m_bgPosition = { 0.0f, 0.0f };
    
    // Finish Logic
    if (t >= 1.0f)
    {
        m_isAnimating = false;
        m_hasTriggered = true;

        // AI Takes over
        if (paddle)
        {
            paddle->SetAIEnabled(true);     // Enable Auto-Pilot
        }
        if (player)
        {
            player->SetBreakoutMode(true);
            player->SetInputEnabled(false); // Disable Keyboard control
        }

    }
}

void SceneGameBreaker::Render(float elapsedTime, Camera* camera)
{
    // ---------------------------------------------------------
    // 1. SETUP & STATE SAVING
    // ---------------------------------------------------------
    Camera* targetCam = camera ? camera : mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // Simpan Back Buffer asli (Layar Monitor)
    ID3D11RenderTargetView* originalRTV = nullptr;
    ID3D11DepthStencilView* originalDSV = nullptr;
    dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);

    // Lepas texture dari shader resource slot agar tidak crash (Read-Write hazard)
    ID3D11ShaderResourceView* nullSRVs[16] = { nullptr };
    dc->PSSetShaderResources(0, 16, nullSRVs);

    // ---------------------------------------------------------
    // 2. DETERMINE RENDER TARGET (LOGIC REFACTOR)
    // ---------------------------------------------------------
    // Kita gunakan boolean Master Switch dari struct m_fxState
    bool usePostProcess = m_fxState.MasterEnabled;

    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DepthStencilView* currentDSV = nullptr;

    if (usePostProcess)
    {
        // Jika Efek NYALA: Gambar ke Texture Khusus (Off-Screen)
        currentRTV = renderTargetView.Get();
        currentDSV = depthStencilView.Get();
    }
    else
    {
        // Jika Efek MATI: Gambar langsung ke Layar
        currentRTV = originalRTV;
        currentDSV = originalDSV;
    }

    // Bind target yang sudah dipilih
    dc->OMSetRenderTargets(1, &currentRTV, currentDSV);

    // Jika menggambar ke Texture Off-Screen, kita harus bersihkan dulu (Clear)
    if (usePostProcess)
    {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Hitam transparan
        dc->ClearRenderTargetView(currentRTV, clearColor);
        dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    // ---------------------------------------------------------
    // 3. RENDER GAME SCENE (Background & 3D Objects)
    // ---------------------------------------------------------

    // --- A. Background Sprite (2D) ---
    if (m_backgroundSprite)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

        float screenW = 1280.0f;
        float screenH = 720.0f;
        if (auto window = Framework::Instance()->GetMainWindow())
        {
            screenW = static_cast<float>(window->GetWidth());
            screenH = static_cast<float>(window->GetHeight());
        }

        m_backgroundSprite->Render(
            dc, 0, 0, 0, screenW, screenH, m_bgRotation,
            bgSpriteColor.x, bgSpriteColor.y, bgSpriteColor.z, bgSpriteColor.w
        );
    }

    // --- B. 3D Objects ---
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // --- C. Debug Shapes ---
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }

    // ---------------------------------------------------------
    // 4. POST-PROCESSING PASS (LOGIC REFACTOR)
    // ---------------------------------------------------------
    if (usePostProcess)
    {
        // A. Kembali ke Back Buffer Asli (Layar)
        dc->OMSetRenderTargets(0, nullptr, nullptr);
        dc->OMSetRenderTargets(1, &originalRTV, originalDSV);

        // B. Setup State untuk menggambar Full Screen Quad
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

        // =========================================================
        // [IMPORTANT] DATA FILTERING LOGIC
        // Disini kita memanipulasi data sebelum dikirim ke GPU
        // tanpa merusak data asli di GUI.
        // =========================================================

        // 1. Salin data asli dari GUI ke variabel temporary
        VignetteShader::VignetteData finalParams = vignetteParams;
        finalParams.time = m_globalTime; // Selalu update waktu

        // 2. Cek filter "Vignette"
        if (!m_fxState.EnableVignette)
        {
            finalParams.intensity = 0.0f; // Matikan efek visualnya
            // Note: Kita tidak mengubah vignetteParams.intensity, jadi slider aman!
        }

        // 3. Cek filter "Lens Distortion"
        if (!m_fxState.EnableLens)
        {
            finalParams.glitchStrength = 0.0f; // Fisheye
            finalParams.distortion = 0.0f; // Edge warp
            finalParams.blurStrength = 0.0f; // Chromatic Ab.
        }

        // 4. Cek filter "CRT / Scanline"
        if (!m_fxState.EnableCRT)
        {
            finalParams.scanlineStrength = 0.0f;
            finalParams.fineOpacity = 0.0f;
        }

        // 5. Override Logika Animasi (Rolling Bar)
        //if (m_fxState.EnableCRTRolling && m_fxState.EnableCRT)
        //{
        //    // Saat mode "On Air/Glitch" aktif, paksa scanline jadi tebal
        //    finalParams.scanlineStrength = 0.8f;
        //}

        // =========================================================

        // C. Kirim data yang SUDAH DIFILTER (finalParams) ke Shader
        // Texture hasil render scene tadi (renderTargetTexture) dipakai sebagai input shader.
        vignetteShader->Draw(dc, shaderResourceView.Get(), finalParams);

        // D. Bersihkan input resource
        dc->PSSetShaderResources(0, 1, nullSRVs);
    }

    // ---------------------------------------------------------
    // 5. CLEANUP
    // ---------------------------------------------------------
    if (originalRTV) originalRTV->Release();
    if (originalDSV) originalDSV->Release();
}

void SceneGameBreaker::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    if (ball) ball->Render(modelRenderer);
    if (blockManager) blockManager->Render(modelRenderer);
    if (paddle) paddle->Render(modelRenderer);
    if (player) player->Render(modelRenderer);

    modelRenderer->Render(rc);
}

void SceneGameBreaker::CreateRenderTarget()
{
    auto device = Graphics::Instance().GetDevice();

    float screenW = 1280.0f;
    float screenH = 720.0f;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // Create Texture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<UINT>(screenW);
    textureDesc.Height = static_cast<UINT>(screenH);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    device->CreateTexture2D(&textureDesc, nullptr, renderTargetTexture.ReleaseAndGetAddressOf());

    // Create RTV & SRV
    device->CreateRenderTargetView(renderTargetTexture.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(renderTargetTexture.Get(), nullptr, shaderResourceView.ReleaseAndGetAddressOf());

    // Create Depth Buffer
    textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&textureDesc, nullptr, depthStencilTexture.ReleaseAndGetAddressOf());
    device->CreateDepthStencilView(depthStencilTexture.Get(), nullptr, depthStencilView.ReleaseAndGetAddressOf());
}

// SceneGameBreaker.cpp

void SceneGameBreaker::DrawGUI()
{
    // Render Debug external (jika ada)
    CameraController::Instance().DrawDebugGUI();

    // Setup Window Utama
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scene Inspector", nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::BeginTabBar("InspectorTabs"))
        {
            // TAB 1: CAMERA CONTROLS
            if (ImGui::BeginTabItem("Camera & Motion"))
            {
                GUICameraTab();
                ImGui::EndTabItem();
            }

            // TAB 2: POST PROCESSING
            if (ImGui::BeginTabItem("Post-Process & FX"))
            {
                GUIPostProcessTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// =========================================================
// HELPER: CAMERA TAB
// =========================================================
void SceneGameBreaker::GUICameraTab()
{
    auto& camCtrl = CameraController::Instance();

    // --- STATUS DISPLAY ---
    ImGui::Spacing();
    bool isBusy = camCtrl.IsTransitioning();
    float progress = 0.0f; // Jika camCtrl punya GetProgress(), bisa dipasang disini

    // Tampilan Status Bar
    ImGui::BeginGroup();
    {
        ImGui::Text("Status:"); ImGui::SameLine();
        if (isBusy)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[ TRANSITIONING ]");
        else
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[ IDLE ]");
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // --- TRANSITION SETTINGS ---
    GUISectionHeader("Transition Settings");

    ImGui::DragFloat("Duration (s)", &transitionDuration, 0.1f, 0.1f, 10.0f, "%.1fs");

    const char* easingItems[] = { "Linear", "EaseIn", "EaseOut", "SmoothStep" };
    ImGui::Combo("Easing Type", &currentEasingIndex, easingItems, IM_ARRAYSIZE(easingItems));

    ImGui::Spacing();
    ImGui::Separator();

    // --- KEYFRAME SELECTOR ---
    GUISectionHeader("Keyframe Targets");

    static int selectedCamIdx = 0;

    // Gunakan Button Grid daripada Radio Button biasa agar lebih rapi
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    if (ImGui::RadioButton("Pose A (Start)", &selectedCamIdx, 0)) {} ImGui::SameLine();
    if (ImGui::RadioButton("Pose B (Mid)", &selectedCamIdx, 1)) {} ImGui::SameLine();
    if (ImGui::RadioButton("Pose C (End)", &selectedCamIdx, 2)) {}
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Action Buttons (Full Width)
    if (ImGui::Button("Move To Selected Pose", ImVec2(-1, 0)))
    {
        EasingType type = static_cast<EasingType>(currentEasingIndex);
        std::vector<CameraKeyframe> path;

        if (auto currentCam = camCtrl.GetActiveCamera())
        {
            path.emplace_back(currentCam->GetPosition(), currentCam->GetRotation());
        }

        switch (selectedCamIdx) {
        case 0: path.push_back(m_poseA); break;
        case 1: path.push_back(m_poseB); break;
        case 2: path.push_back(m_poseC); break;
        }
        camCtrl.StartPathTransition(path, transitionDuration, type);
    }

    if (ImGui::Button("Play Sequence (A -> B -> C)", ImVec2(-1, 0)))
    {
        std::vector<CameraKeyframe> path = { m_poseA, m_poseB, m_poseC };
        camCtrl.StartPathTransition(path, transitionDuration, static_cast<EasingType>(currentEasingIndex));
        selectedCamIdx = 2; // Auto select end
    }

    ImGui::Spacing();
    ImGui::Separator();

    // --- LIVE EDITOR ---
    CameraKeyframe* targetPose = nullptr;
    const char* label = "None";
    ImVec4 labelColor = ImVec4(1, 1, 1, 1);

    switch (selectedCamIdx) {
    case 0: targetPose = &m_poseA; label = "POSE A"; labelColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); break;
    case 1: targetPose = &m_poseB; label = "POSE B"; labelColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); break;
    case 2: targetPose = &m_poseC; label = "POSE C"; labelColor = ImVec4(1.0f, 0.6f, 0.4f, 1.0f); break;
    }

    if (targetPose)
    {
        GUISectionHeader("Live Editor");
        ImGui::TextColored(labelColor, "EDITING: %s", label);

        ImGui::Indent();
        bool changed = false;

        if (ImGui::DragFloat3("Position", &targetPose->Position.x, 0.05f)) changed = true;
        if (ImGui::DragFloat3("Target LookAt", &targetPose->TargetLookAt.x, 0.05f)) changed = true;

        float rollDeg = XMConvertToDegrees(targetPose->Rotation.z);
        if (ImGui::SliderFloat("Roll Angle", &rollDeg, -180.0f, 180.0f, "%.0f deg")) {
            targetPose->Rotation.z = XMConvertToRadians(rollDeg);
            changed = true;
        }
        ImGui::Unindent();

        // WYSIWYG Logic
        if (changed || ImGui::IsItemActive())
        {
            // Update temporary logic
            mainCamera->SetPosition(targetPose->Position);
            mainCamera->LookAt(targetPose->TargetLookAt);

            XMFLOAT3 calcRot = mainCamera->GetRotation();
            calcRot.z = targetPose->Rotation.z;

            // Simpan kembali rotasi yang sudah dihitung
            targetPose->Rotation = calcRot;

            // Update visual controller
            camCtrl.SetFixedSetting(targetPose->Position);
            camCtrl.SetTarget(targetPose->TargetLookAt);
            camCtrl.SetFixedRollOffset(calcRot.z);

            mainCamera->SetRotation(calcRot);
        }
    }
}

// =========================================================
// HELPER: POST PROCESS TAB
// =========================================================
void SceneGameBreaker::GUIPostProcessTab()
{
    // =========================================================
    // MASTER SWITCH
    // =========================================================
    ImGui::Spacing();

    if (m_fxState.MasterEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Turn Off Filter", ImVec2(-1, 40))) {
            m_fxState.MasterEnabled = false;
        }
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Turn On Filter", ImVec2(-1, 40))) {
            m_fxState.MasterEnabled = true;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    if (!m_fxState.MasterEnabled)
    {
        ImGui::TextDisabled("Post-processing pipeline is bypassed.");
        return;
    }

    // =========================================================
    // 1. VIGNETTE & COLOR GRADING
    // =========================================================
    if (ImGui::CollapsingHeader("Vignette & Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Checkbox Utama
        ImGui::Checkbox("ACTIVATE: Vignette Layer", &m_fxState.EnableVignette);
        ImGui::Spacing();

        // --- PENGGANTI BeginDisabled ---
        // Jika fitur MATI, kita buat tampilannya jadi transparan (0.5) agar terlihat "Disabled"
        if (!m_fxState.EnableVignette) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        // Code Slider tetap sama
        ImGui::ColorEdit3("Tint Color", &vignetteParams.color.x);
        ImGui::SliderFloat("Intensity", &vignetteParams.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &vignetteParams.smoothness, 0.01f, 1.0f);
        ImGui::SliderFloat2("Center", &vignetteParams.center.x, 0.0f, 1.0f);

        ImGui::Spacing();
        ImGui::Checkbox("Rounded Mask", &vignetteParams.rounded);
        if (vignetteParams.rounded) {
            ImGui::SliderFloat("Roundness", &vignetteParams.roundness, 0.0f, 1.0f);
        }

        // --- PENGGANTI EndDisabled ---
        // Kembalikan transparansi ke normal
        if (!m_fxState.EnableVignette) ImGui::PopStyleVar();

        ImGui::Unindent();
    }

    // =========================================================
    // 2. LENS DISTORTION (FISHEYE)
    // =========================================================
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        ImGui::Indent();

        ImGui::Checkbox("ACTIVATE: Lens Layer", &m_fxState.EnableLens);
        ImGui::Spacing();

        // --- Manual Disable Effect ---
        if (!m_fxState.EnableLens) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Geometry");
        ImGui::SliderFloat("Fisheye", &vignetteParams.glitchStrength, 0.0f, 1.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Aberration");
        ImGui::SliderFloat("Chroma Blur", &vignetteParams.blurStrength, 0.0f, 0.05f, "%.4f");

        if (!m_fxState.EnableLens) ImGui::PopStyleVar();
        // -----------------------------

        ImGui::Unindent();
    }

    // =========================================================
    // 3. CRT & SCANLINES
    // =========================================================
    if (ImGui::CollapsingHeader("CRT Monitor Effect"))
    {
        ImGui::Indent();

        ImGui::Checkbox("ACTIVATE: CRT Layer", &m_fxState.EnableCRT);
        ImGui::Spacing();

        // --- Manual Disable Effect ---
        if (!m_fxState.EnableCRT) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Scanlines");
        ImGui::SliderFloat("Density", &vignetteParams.fineDensity, 10.0f, 500.0f);
        ImGui::SliderFloat("Opacity ", &vignetteParams.fineOpacity, 0.0f, 1.0f);

        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Rolling Scanline");
        ImGui::SliderFloat("Speed", &vignetteParams.scanlineSpeed, -10.0f, 10.0f);
        ImGui::SliderFloat("Size", &vignetteParams.scanlineSize, 1.0f, 150.0f);
        ImGui::SliderFloat("Opacity", &vignetteParams.scanlineStrength, 0.0f, 1.0f);
        
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Distortion");
        ImGui::SliderFloat("CRT Distortion", &vignetteParams.distortion, -0.5f, 0.5f);


        if (!m_fxState.EnableCRT) ImGui::PopStyleVar();
        // -----------------------------

        ImGui::Unindent();
    }
}

// =========================================================
// HELPER: UTILITY
// =========================================================
void SceneGameBreaker::GUISectionHeader(const char* label)
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", label);
    ImGui::Separator();
    ImGui::Spacing();
}

void SceneGameBreaker::OnResize(int width, int height)
{
    if (height == 0) height = 1;
    if (mainCamera)
    {
        mainCamera->SetAspectRatio((float)width / (float)height);
    }
    CreateRenderTarget();
}