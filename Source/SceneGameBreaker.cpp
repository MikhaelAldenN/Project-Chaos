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
    float screenW = 1920;
    float screenH = 1080;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // 1. Setup Main Camera
    mainCamera = std::make_shared<Camera>();
    mainCamera->SetPerspectiveFov(XMConvertToRadians(initialFOV), screenW / screenH, cameraNearZ, cameraFarZ);

    // Set Controller
    auto& camCtrl = CameraController::Instance();
    camCtrl.SetActiveCamera(mainCamera);

    // Setup Posisi Awal (Static)
    XMFLOAT3 startPos = cameraPosition;
    startPos.x = 0.0f;
    startPos.z = 0.0f;
    startPos.y = 20.0f;

    mainCamera->SetPosition(startPos);
    mainCamera->LookAt(cameraTarget);

    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
    camCtrl.SetFixedSetting(startPos);
    camCtrl.SetTarget(cameraTarget);


    // --------------------------------------------------------
    // SETUP CAMERA SCENARIO (A, B, C)
    // --------------------------------------------------------
    // Point A (Posisi Awal)
    m_camScenarioPoints.push_back({ "Point A (Start)", {0.0f, 20.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 2.0f, EasingType::SmoothStep });

    // Point B (Turun sedikit)
    m_camScenarioPoints.push_back({ "Point B (Mid)",   {0.7f, 15.0f, 0.0f}, {-0.9f, 0.0f, 2.8f}, 2.0f, EasingType::SmoothStep });

    // Point C (Close Up)
    m_camScenarioPoints.push_back({ "Point C (Low)",   {2.3f, 11.0f, 5.2f},  {-0.8f, 0.0f, 2.0f}, 3.0f, EasingType::EaseOutQuad });

    m_camScenarioPoints.push_back({ "Point D (Top Wide)", {0.0f, 10.0f, 10.1f}, {-0.2f, 0.0f, 2.1f}, 4.0f, EasingType::SmoothStep });

    // --------------------------------------------------------
    // Initialize Assets
    // --------------------------------------------------------
    m_spriteBorderBreaker = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathBorderBreaker);
	m_spriteDEBUG_LAYOUT = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathDebugLayout);

    ball = new Ball();
    paddle = new Paddle();
    paddle->SetAIEnabled(false);
    player = new Player();
    player->SetInputEnabled(false);
    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player);

    uberShader = std::make_unique<UberShader>(Graphics::Instance().GetDevice());
    CreateRenderTarget();
}

SceneGameBreaker::~SceneGameBreaker()
{
    CameraController::Instance().ClearCamera();
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
    if (m_hasTriggered) return;

    // Check if blocks are low enough to trigger Breakout Mode
    // Tanpa animasi kamera, langsung ubah state game
    bool triggerCondition = (GetKeyState('T') & 0x8000)
        || (blockManager && blockManager->GetActiveBlockCount() <= triggerBlockCount);

    if (triggerCondition)
    {
        m_hasTriggered = true;

        // Enable AI / Breakout Mode
        if (paddle)
        {
            paddle->SetAIEnabled(true);
        }
        if (player)
        {
            player->SetBreakoutMode(true);
            player->SetInputEnabled(false);
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

    ID3D11RenderTargetView* originalRTV = nullptr;
    ID3D11DepthStencilView* originalDSV = nullptr;
    dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);

    ID3D11ShaderResourceView* nullSRVs[16] = { nullptr };
    dc->PSSetShaderResources(0, 16, nullSRVs);

    // ---------------------------------------------------------
    // 2. DETERMINE RENDER TARGET
    // ---------------------------------------------------------
    bool usePostProcess = m_fxState.MasterEnabled;
    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DepthStencilView* currentDSV = nullptr;

    if (usePostProcess)
    {
        currentRTV = renderTargetView.Get();
        currentDSV = depthStencilView.Get();
    }
    else
    {
        currentRTV = originalRTV;
        currentDSV = originalDSV;
    }

    dc->OMSetRenderTargets(1, &currentRTV, currentDSV);

    if (usePostProcess)
    {
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        dc->ClearRenderTargetView(currentRTV, clearColor);
        dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    // ---------------------------------------------------------
    // 3. RENDER GAME SCENE
    // ---------------------------------------------------------

    if (m_spriteDEBUG_LAYOUT)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

        float screenW = 1920.0f;
        float screenH = 1080.0f;
        if (auto window = Framework::Instance()->GetMainWindow())
        {
            screenW = static_cast<float>(window->GetWidth());
            screenH = static_cast<float>(window->GetHeight());
        }

        m_spriteDEBUG_LAYOUT->Render(
            dc,
            0.0f, 0.0f, 0.0f,
            screenW, screenH,
            0.0f,
            1.0f, 1.0f, 1.0f, 0.5f
        );
    }

    // --- A. 3D Objects (Opaque) ---
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    // --- B. World Space Sprite (Transparent) ---
    if (m_spriteBorderBreaker)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

        float worldX = m_spritePos.x;
        float worldY = m_spritePos.y;
        float worldZ = m_spritePos.z;

        float finalW = m_spriteSize.x * m_spriteScale;
        float finalH = m_spriteSize.y * m_spriteScale;

        float pitch = XMConvertToRadians(m_spritePitch);
        float yaw = XMConvertToRadians(m_spriteYaw);
        float totalRoll = m_spriteRoll + m_bgRotation;
        float roll = XMConvertToRadians(totalRoll);

        m_spriteBorderBreaker->Render(
            dc,
            targetCam,
            worldX, worldY, worldZ,
            finalW, finalH,
            pitch, yaw, roll,
            bgSpriteColor.x, bgSpriteColor.y, bgSpriteColor.z, bgSpriteColor.w
        );
    }

    // --- C. Debug Shapes ---
    if (targetCam == mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }

    // ---------------------------------------------------------
    // 4. POST-PROCESSING PASS
    // ---------------------------------------------------------
    if (usePostProcess)
    {
        dc->OMSetRenderTargets(0, nullptr, nullptr);
        dc->OMSetRenderTargets(1, &originalRTV, originalDSV);

        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

        UberShader::UberData finalParams = uberParams;
        finalParams.time = m_globalTime;

        if (!m_fxState.EnableVignette) finalParams.intensity = 0.0f;

        if (!m_fxState.EnableLens)
        {
            finalParams.glitchStrength = 0.0f;
            finalParams.distortion = 0.0f;
            finalParams.blurStrength = 0.0f;
        }

        if (!m_fxState.EnableCRT)
        {
            finalParams.scanlineStrength = 0.0f;
            finalParams.fineOpacity = 0.0f;
        }

        uberShader->Draw(dc, shaderResourceView.Get(), finalParams);
        dc->PSSetShaderResources(0, 1, nullSRVs);
    }

    // 5. Cleanup
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

    float screenW = 1920;
    float screenH = 1080;
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

// SceneGameBreaker.cpp GUI
void SceneGameBreaker::DrawGUI()
{
    // 1. Debug Controller (Window Terpisah)
    CameraController::Instance().DrawDebugGUI();

    // 2. Window Utama "Scene Inspector"
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    // KITA BUKA WINDOW DI SINI
    if (ImGui::Begin("Scene Inspector", nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::BeginTabBar("InspectorTabs"))
        {
            // --- TAB 1: CAMERA ---
            if (ImGui::BeginTabItem("Camera Info"))
            {
                // PENTING: Panggil GUICameraTab DI SINI!
                // (Karena dia butuh window "Scene Inspector" yang sedang aktif)
                GUICameraTab();

                ImGui::EndTabItem();
            }

            // --- TAB 2: POST PROCESS ---
            if (ImGui::BeginTabItem("Post-Process & FX"))
            {
                GUIPostProcessTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    // KITA TUTUP WINDOW DI SINI
    ImGui::End();


    // 3. Window Sprite (Window Terpisah)
    // Panggil ini DI LUAR window Scene Inspector karena dia bikin window sendiri
    GUISpriteTab();
}

void SceneGameBreaker::GUICameraTab()
{
    auto& camCtrl = CameraController::Instance();

    static float moveSpeed = 3.5f;
    static bool useConstantSpeed = true;
    static bool useSmoothCurve = true;
    static int guiEasingIdx = 0;

    // Default pilih SmoothStep agar efek "Satu Sequence" langsung terasa
    static int globalEasingSelection = (int)EasingType::SmoothStep;
    static float curveTension = 1.2f;

    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "SEQUENCE EDITOR");
    ImGui::Separator();

    ImGui::Checkbox("Use Constant Speed Mode", &useConstantSpeed);

    if (useConstantSpeed)
    {
        ImGui::DragFloat("Travel Speed", &moveSpeed, 0.1f, 0.1f, 50.0f);

        // List Item untuk Combo Box
        const char* easingItems[] = {
            "Linear",                   // Index 0
            "EaseIn (Cubic)",           // Index 1
            "EaseOut (Cubic)",          // Index 2
            "Smooth Sequence (Auto)"    // Index 3 -> KITA MAU INI JADI SequenceAutoCubic
        };

        ImGui::Combo("Sequence Easing", &guiEasingIdx, easingItems, IM_ARRAYSIZE(easingItems));

        // Spline Options
        ImGui::Checkbox("Enable Smooth Curve (Spline)", &useSmoothCurve);

        if (useSmoothCurve) {
            ImGui::Indent();
            // TAMBAHKAN SLIDER INI
            if (ImGui::DragFloat("Curve Tension", &curveTension, 0.05f, 0.0f, 3.0f))
            {
                camCtrl.SetSplineTension(curveTension);
            }
            // Penjelasan visual
            if (curveTension < 1.0f) ImGui::TextDisabled("(Tighter / Sharper turns)");
            else if (curveTension > 1.0f) ImGui::TextDisabled("(Wider / Rounder turns)");

            ImGui::Unindent();
        }
    }

    ImGui::Spacing();

    if (ImGui::Button(useConstantSpeed ? "PLAY SEQUENCE" : "PLAY MANUAL", ImVec2(-1, 40)))
    {
        camCtrl.SetSplineTension(curveTension);
        std::vector<CameraKeyframe> sequence;

        for (const auto& point : m_camScenarioPoints)
        {
            CameraKeyframe key;
            key.TargetPosition = point.Position;
            key.TargetRotation = CalculateRotationFromTarget(point.Position, point.LookAtTarget);

            // Manual value (akan di-override jika mode Constant Speed aktif)
            key.Duration = point.Duration;
            key.Easing = point.Easing;

            sequence.push_back(key);
        }
        if (useConstantSpeed)
        {
            // MAPPING DARI GUI INDEX KE ENUM YANG BENAR
            EasingType finalEasing = EasingType::Linear;

            switch (guiEasingIdx)
            {
            case 0: finalEasing = EasingType::Linear; break;
            case 1: finalEasing = EasingType::EaseInCubic; break;
            case 2: finalEasing = EasingType::EaseOutCubic; break;
            case 3: finalEasing = EasingType::SequenceAutoCubic; break; // <--- INI KUNCINYA
            }

            camCtrl.SetSplineTension(curveTension);

            // Kirim finalEasing yang sudah dimapping
            camCtrl.PlaySequenceBySpeed(sequence, moveSpeed, finalEasing, useSmoothCurve, false);
        }
        else
        {
            camCtrl.PlaySequence(sequence, false);
        }
    }

    // ... (Tombol Stop & Reset tetap sama) ...
    if (ImGui::Button("Stop & Reset Camera"))
    {
        camCtrl.StopSequence();
        if (!m_camScenarioPoints.empty()) {
            camCtrl.SetFixedSetting(m_camScenarioPoints[0].Position);
            camCtrl.SetTarget(m_camScenarioPoints[0].LookAtTarget);
        }
    }
    // ...

    ImGui::Spacing();
    ImGui::Separator();

    // --- EDITOR LIST ---
    for (int i = 0; i < m_camScenarioPoints.size(); ++i)
    {
        auto& pt = m_camScenarioPoints[i];
        ImGui::PushID(i);

        if (ImGui::CollapsingHeader(pt.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            // Variabel untuk mendeteksi perubahan
            bool isEdited = false;

            ImGui::Text("Transform");

            // Cek apakah Position berubah?
            if (ImGui::DragFloat3("Position", &pt.Position.x, 0.1f)) {
                isEdited = true;
            }

            // Cek apakah LookAt berubah?
            if (ImGui::DragFloat3("Look At", &pt.LookAtTarget.x, 0.1f)) {
                isEdited = true;
            }

            // --- LOGIKA UPDATE LIVE ---
            // Jika ada perubahan, langsung paksa kamera pindah ke titik ini
            if (isEdited)
            {
                camCtrl.StopSequence(); // Hentikan animasi jika sedang jalan
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);

                // Update Posisi Controller
                camCtrl.SetFixedSetting(pt.Position);
                camCtrl.SetTarget(pt.LookAtTarget);

                // Update Posisi Kamera Asli (Biar instan tanpa lag 1 frame)
                if (auto mainCam = GetMainCamera()) {
                    mainCam->SetPosition(pt.Position);
                    mainCam->LookAt(pt.LookAtTarget);
                }
            }

            // HANYA TAMPILKAN OPSI TIMING JIKA MODE MANUAL
            if (!useConstantSpeed)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Manual Timing Settings");
                ImGui::DragFloat("Duration (sec)", &pt.Duration, 0.1f, 0.1f, 10.0f);

                const char* easingItems[] = { "Linear", "EaseIn", "EaseOut", "SmoothStep" };
                int currentEasing = (int)pt.Easing;
                if (ImGui::Combo("Easing", &currentEasing, easingItems, IM_ARRAYSIZE(easingItems)))
                {
                    pt.Easing = (EasingType)currentEasing;
                }
            }
            else
            {
                ImGui::Spacing();
                ImGui::TextDisabled("[Auto-Calculated in Constant Speed Mode]");
            }

            // Tombol Jump to View tetap ada
            if (ImGui::Button("Jump to this view"))
            {
                camCtrl.StopSequence();
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                camCtrl.SetFixedSetting(pt.Position);
                camCtrl.SetTarget(pt.LookAtTarget);
                if (auto mainCam = GetMainCamera()) {
                    mainCam->SetPosition(pt.Position);
                    mainCam->LookAt(pt.LookAtTarget);
                }
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
        ImGui::Spacing();
    }
}

void SceneGameBreaker::GUIPostProcessTab()
{
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

    // Vignette
    if (ImGui::CollapsingHeader("Vignette & Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Vignette Layer", &m_fxState.EnableVignette);

        if (!m_fxState.EnableVignette) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::ColorEdit3("Tint Color", &uberParams.color.x);
        ImGui::SliderFloat("Intensity", &uberParams.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &uberParams.smoothness, 0.01f, 1.0f);
        ImGui::SliderFloat2("Center", &uberParams.center.x, 0.0f, 1.0f);

        ImGui::Checkbox("Rounded Mask", &uberParams.rounded);
        if (uberParams.rounded) ImGui::SliderFloat("Roundness", &uberParams.roundness, 0.0f, 1.0f);

        if (!m_fxState.EnableVignette) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    // Lens
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Lens Layer", &m_fxState.EnableLens);

        if (!m_fxState.EnableLens) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::SliderFloat("Fisheye", &uberParams.distortion, -0.5f, 0.5f);
        ImGui::SliderFloat("Chroma Blur", &uberParams.blurStrength, 0.0f, 0.05f, "%.4f");
        ImGui::SliderFloat("Glitch Shake", &uberParams.glitchStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableLens) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    // CRT
    if (ImGui::CollapsingHeader("CRT Monitor Effect"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: CRT Layer", &m_fxState.EnableCRT);

        if (!m_fxState.EnableCRT) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::SliderFloat("Density", &uberParams.fineDensity, 10.0f, 500.0f);
        ImGui::SliderFloat("Opacity ", &uberParams.fineOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Speed", &uberParams.scanlineSpeed, -10.0f, 10.0f);
        ImGui::SliderFloat("Size", &uberParams.scanlineSize, 1.0f, 150.0f);
        ImGui::SliderFloat("Scanline Opacity", &uberParams.scanlineStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableCRT) ImGui::PopStyleVar();
        ImGui::Unindent();
    }
}

void SceneGameBreaker::GUISectionHeader(const char* label)
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", label);
    ImGui::Separator();
    ImGui::Spacing();
}

void SceneGameBreaker::GUISpriteTab()
{
    ImGui::Begin("Sprite Inspector");

    if (ImGui::CollapsingHeader("World Decoration (Sprite 3D)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "TRANSFORM");
        ImGui::DragFloat3("Position", &m_spritePos.x, 0.1f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SIZE & RATIO");

        ImGui::DragFloat("Master Scale (Total Size)", &m_spriteScale, 0.1f, 0.0f, 100.0f);
        ImGui::Text("Base Aspect Ratio (Shape)");
        ImGui::DragFloat2("W : H", &m_spriteSize.x, 0.1f);

        if (ImGui::Button("Set 16:9")) m_spriteSize = { 16.0f, 9.0f };
        ImGui::SameLine();
        if (ImGui::Button("Set 4:3")) m_spriteSize = { 4.0f, 3.0f };
        ImGui::SameLine();
        if (ImGui::Button("Set 1:1")) m_spriteSize = { 10.0f, 10.0f };

        ImGui::TextDisabled("Final Size: %.1f x %.1f", m_spriteSize.x * m_spriteScale, m_spriteSize.y * m_spriteScale);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "ORIENTATION");
        ImGui::SliderFloat("Pitch", &m_spritePitch, -180.0f, 180.0f);
        ImGui::SliderFloat("Yaw", &m_spriteYaw, -180.0f, 180.0f);
        ImGui::SliderFloat("Roll", &m_spriteRoll, -180.0f, 180.0f);

        ImGui::Unindent();
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

DirectX::XMFLOAT3 SceneGameBreaker::CalculateRotationFromTarget(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target)
{
    // Logika matematika yang sama dengan Camera::LookAt
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vTarget = XMLoadFloat3(&target);
    XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
    vDir = XMVector3Normalize(vDir);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);

    float pitch = asinf(-dir.y);
    float yaw = atan2f(dir.x, dir.z);

    return { pitch, yaw, 0.0f }; // Return Euler Angles
}