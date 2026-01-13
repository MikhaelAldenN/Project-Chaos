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

    // Save the current Back Buffer (Screen) and Depth Buffer
    ID3D11RenderTargetView* originalRTV = nullptr;
    ID3D11DepthStencilView* originalDSV = nullptr;
    dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);

    // Unbind all shader resources (Textures) to prevent "Read-While-Writing" hazards
    ID3D11ShaderResourceView* nullSRVs[16] = { nullptr };
    dc->PSSetShaderResources(0, 16, nullSRVs);

    // ---------------------------------------------------------
    // 2. DETERMINE RENDER TARGET
    // ---------------------------------------------------------
    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DepthStencilView* currentDSV = nullptr;

    if (vignetteParams.enabled)
    {
        // Post-Process Enabled
        currentRTV = renderTargetView.Get();
        currentDSV = depthStencilView.Get();
    }
    else
    {
        // Post-Process Disabled
        currentRTV = originalRTV;
        currentDSV = originalDSV;
    }

    // Bind the chosen target
    dc->OMSetRenderTargets(1, &currentRTV, currentDSV);

    // If using the Off-Screen Texture, clear it manually
    if (vignetteParams.enabled)
    {
        // Clear to Opaque Black
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        dc->ClearRenderTargetView(currentRTV, clearColor);
        dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    // ---------------------------------------------------------
    // 3. RENDER GAME SCENE (Background & 3D Objects)
    // ---------------------------------------------------------

    // --- Background Sprite (2D) ---
    if (m_backgroundSprite)
    {
        // Setup states for 2D transparency
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

        // Get current screen dimensions
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

    // --- Pass 2: 3D Objects ---
    // Restore states for standard 3D rendering
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // --- Debug Shapes (Main Camera Only) ---
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }

    // ---------------------------------------------------------
    // 4. POST-PROCESSING 
    // ---------------------------------------------------------
    // Only execute this pass if the effect is enabled
    if (vignetteParams.enabled)
    {
        // Switch back to the Screen (Back Buffer)
        dc->OMSetRenderTargets(0, nullptr, nullptr); // Unbind current target
        dc->OMSetRenderTargets(1, &originalRTV, originalDSV);

        // Setup states for full-screen copy
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

        // Pass the timer to shader
        vignetteParams.time = m_globalTime;

        // Draw the Off-Screen Texture using the Vignette Shader
        vignetteShader->Draw(dc, shaderResourceView.Get(), vignetteParams);

        // Cleanup resources
        dc->PSSetShaderResources(0, 1, nullSRVs);
    }

    // ---------------------------------------------------------
    // 5. CLEANUP
    // ---------------------------------------------------------
    // Release references incremented by OMGetRenderTargets
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

void SceneGameBreaker::DrawGUI()
{
    auto& camCtrl = CameraController::Instance();
    camCtrl.DrawDebugGUI();

    ImGui::Begin("Game Breaker Debug");

    if (ImGui::CollapsingHeader("Camera Control Center", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool isBusy = camCtrl.IsTransitioning();
        ImGui::TextColored(isBusy ? ImVec4(1, 1, 0, 1) : ImVec4(1, 1, 1, 1),
            "Status: %s", isBusy ? "TRANSITIONING..." : "IDLE");

        ImGui::SliderFloat("Transition Time", &transitionDuration, 0.1f, 5.0f);
        const char* easingItems[] = { "Linear", "EaseIn", "EaseOut", "SmoothStep" };
        ImGui::Combo("Easing", &currentEasingIndex, easingItems, IM_ARRAYSIZE(easingItems));

        ImGui::Separator();

        static int selectedCamIdx = 0;
        ImGui::Text("Select Target Data:");
        ImGui::RadioButton("Pose A", &selectedCamIdx, 0); ImGui::SameLine();
        ImGui::RadioButton("Pose B", &selectedCamIdx, 1); ImGui::SameLine();
        ImGui::RadioButton("Pose C", &selectedCamIdx, 2);

        ImGui::Spacing();

        // [UPDATE] Menggunakan StartPathTransition dengan Vector of Structs
        if (ImGui::Button("Go To Selected Pose", ImVec2(-1, 30)))
        {
            EasingType type = static_cast<EasingType>(currentEasingIndex);
            std::vector<CameraKeyframe> path;

            // Masukkan posisi awal (Current Camera) sebagai titik start
            if (auto currentCam = camCtrl.GetActiveCamera())
            {
                path.emplace_back(currentCam->GetPosition(), currentCam->GetRotation());
            }

            // Masukkan target
            switch (selectedCamIdx) {
            case 0: path.push_back(m_poseA); break;
            case 1: path.push_back(m_poseB); break;
            case 2: path.push_back(m_poseC); break;
            }

            camCtrl.StartPathTransition(path, transitionDuration, type);
        }

        if (ImGui::Button("Play Sequence: A -> B -> C", ImVec2(-1, 30)))
        {
            // Kirim semua struct ke controller
            std::vector<CameraKeyframe> path = { m_poseA, m_poseB, m_poseC };
            camCtrl.StartPathTransition(path, transitionDuration, static_cast<EasingType>(currentEasingIndex));
            selectedCamIdx = 2;
        }

        ImGui::Separator();

        // --------------------------------------------------------
        // LIVE DATA EDITOR (Langsung edit Struct)
        // --------------------------------------------------------
        CameraKeyframe* targetPose = nullptr;
        const char* label = "Unknown";

        switch (selectedCamIdx) {
        case 0: targetPose = &m_poseA; label = "POSE A"; break;
        case 1: targetPose = &m_poseB; label = "POSE B"; break;
        case 2: targetPose = &m_poseC; label = "POSE C"; break;
        }

        if (targetPose)
        {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "EDITING DATA: %s", label);
            bool changed = false;

            // A. Edit Position
            if (ImGui::DragFloat3("Position", &targetPose->Position.x, 0.1f)) changed = true;

            // B. Edit Target LookAt (Data only)
            if (ImGui::DragFloat3("Look At Target", &targetPose->TargetLookAt.x, 0.1f)) changed = true;

            // C. Edit Roll
            float rollDeg = XMConvertToDegrees(targetPose->Rotation.z);
            if (ImGui::SliderFloat("Roll (Tilt)", &rollDeg, -180.0f, 180.0f)) {
                targetPose->Rotation.z = XMConvertToRadians(rollDeg);
                changed = true;
            }

            // WYSIWYG Logic: Update kamera utama jika sedang mengedit
            if (changed || ImGui::IsItemActive())
            {
                // Kita pinjam mainCamera sebentar untuk hitung rotasi LookAt baru
                mainCamera->SetPosition(targetPose->Position);
                mainCamera->LookAt(targetPose->TargetLookAt);

                // Ambil rotasi hasil LookAt, lalu timpa Z-nya dengan settingan Roll kita
                XMFLOAT3 calcRot = mainCamera->GetRotation();
                calcRot.z = targetPose->Rotation.z;

                // Simpan balik ke struct agar konsisten
                targetPose->Rotation = calcRot;

                // Update Controller agar visual di layar berubah
                camCtrl.SetFixedSetting(targetPose->Position);
                camCtrl.SetTarget(targetPose->TargetLookAt);
                camCtrl.SetFixedRollOffset(calcRot.z);

                // Pastikan kamera di posisi yang benar
                mainCamera->SetRotation(calcRot);
            }
        }
    }


    if (ImGui::CollapsingHeader("Vignette Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Use Vignette", &vignetteParams.enabled);

        if (vignetteParams.enabled)
        {
            ImGui::ColorEdit3("Color", &vignetteParams.color.x);
            ImGui::SliderFloat2("Center", &vignetteParams.center.x, 0.0f, 1.0f);
            ImGui::SliderFloat("Intensity", &vignetteParams.intensity, 0.0f, 1.0f);
            ImGui::SliderFloat("Smoothness", &vignetteParams.smoothness, 0.0f, 1.0f);
            ImGui::Checkbox("Rounded", &vignetteParams.rounded);
            ImGui::SliderFloat("Roundness", &vignetteParams.roundness, 0.0f, 1.0f);
            ImGui::Separator();

            ImGui::Text("Lens Effects");
            ImGui::SliderFloat("Blur Amount", &vignetteParams.blurStrength, 0.0f, 0.05f);
            ImGui::SliderFloat("Fish Eye (Distortion)", &vignetteParams.distortion, -0.1f, 0.15f);
            ImGui::Separator();

            ImGui::Text("CRT Effects");
            ImGui::Checkbox("AUTO PLAY GLITCH", &m_isGlitching);
            if (m_isGlitching)
            {
                // 1. SCANLINE / HUM BAR
                // Set to a fixed high visibility.
                // The movement is handled by "Time" in the shader, so we don't need to change this value.
                vignetteParams.scanlineStrength = 0.8f;

                // 2. DISABLE SHAKE (Clean Look)
                // You requested "No shake at all", so we force this to 0.
                vignetteParams.glitchStrength = 0.0f;

                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0, 1, 0, 1), " << ROLLING >> ");
            }

            // Sliders 
            ImGui::SliderFloat("Glitch Shake", &vignetteParams.glitchStrength, 0.0f, 1.0f);
            // ROLLING BAR SETTINGS
            ImGui::Separator();
            ImGui::Text("Rolling Bar (Animation)");
            ImGui::SliderFloat("Roll Opacity", &vignetteParams.scanlineStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Roll Speed", &vignetteParams.scanlineSpeed, -10.0f, 10.0f);
            ImGui::SliderFloat("Roll Sharpness", &vignetteParams.scanlineSize, 1.0f, 100.0f);

            // FINE MESH SETTINGS
            ImGui::Separator();
            ImGui::Text("CRT Mesh (Background)");
            ImGui::SliderFloat("Mesh Opacity", &vignetteParams.fineOpacity, 0.0f, 1.0f);
            ImGui::SliderFloat("Mesh Density", &vignetteParams.fineDensity, 10.0f, 100.0f);
        }
        else
        {
            ImGui::TextDisabled("Settings are hidden while disabled.");
        }
    }

    ImGui::End();
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