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
    m_camScenarioPoints.push_back({ "Point B (Mid)",   {0.0f, 15.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 2.0f, EasingType::SmoothStep });

    // Point C (Close Up)
    m_camScenarioPoints.push_back({ "Point C (Low)",   {0.0f, 5.0f, 0.0f},  {0.0f, 0.0f, 0.0f}, 3.0f, EasingType::EaseOutQuad });

    // --------------------------------------------------------
    // Initialize Assets
    // --------------------------------------------------------
    m_spriteBorderBreaker = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathBorderBreaker);

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
            1.0f, 1.0f, 1.0f, 1.0f
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

        VignetteShader::VignetteData finalParams = vignetteParams;
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

        vignetteShader->Draw(dc, shaderResourceView.Get(), finalParams);
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

    // Static Variable untuk menyimpan settingan GUI
    static float moveSpeed = 5.0f; // Kecepatan kamera (Meter/Detik)
    static bool useConstantSpeed = true; // Checkbox pilihan mode

    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "SEQUENCE EDITOR");
    ImGui::Separator();

    // --- MODE SELECTION ---
    ImGui::Checkbox("Use Constant Speed Mode", &useConstantSpeed);

    if (useConstantSpeed)
    {
        ImGui::DragFloat("Travel Speed (Units/Sec)", &moveSpeed, 0.1f, 0.1f, 50.0f);
        ImGui::TextDisabled("Duration & Easing will be auto-calculated.");
    }
    else
    {
        ImGui::TextDisabled("Using Manual Duration per Keyframe.");
    }

    ImGui::Spacing();

    // --- PLAY BUTTON ---
    std::string playLabel = useConstantSpeed ? "PLAY (Constant Speed)" : "PLAY (Manual Timing)";

    if (ImGui::Button(playLabel.c_str(), ImVec2(-1, 40)))
    {
        std::vector<CameraKeyframe> sequence;

        for (const auto& point : m_camScenarioPoints)
        {
            CameraKeyframe key;
            key.TargetPosition = point.Position;
            key.TargetRotation = CalculateRotationFromTarget(point.Position, point.LookAtTarget);

            // Jika Manual, kita pakai settingan dari list. 
            // Jika ConstantSpeed, nilai ini nanti akan ditimpa/diabaikan oleh fungsi PlaySequenceBySpeed.
            key.Duration = point.Duration;
            key.Easing = point.Easing;

            sequence.push_back(key);
        }

        if (useConstantSpeed)
        {
            // Panggil fungsi BARU
            camCtrl.PlaySequenceBySpeed(sequence, moveSpeed, false);
        }
        else
        {
            // Panggil fungsi LAMA
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
            ImGui::Text("Transform");
            ImGui::DragFloat3("Position", &pt.Position.x, 0.1f);
            ImGui::DragFloat3("Look At", &pt.LookAtTarget.x, 0.1f);

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
                if (auto mainCam = GetMainCamera()) { // Pastikan helper getter ada atau akses variable
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

        ImGui::ColorEdit3("Tint Color", &vignetteParams.color.x);
        ImGui::SliderFloat("Intensity", &vignetteParams.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &vignetteParams.smoothness, 0.01f, 1.0f);
        ImGui::SliderFloat2("Center", &vignetteParams.center.x, 0.0f, 1.0f);

        ImGui::Checkbox("Rounded Mask", &vignetteParams.rounded);
        if (vignetteParams.rounded) ImGui::SliderFloat("Roundness", &vignetteParams.roundness, 0.0f, 1.0f);

        if (!m_fxState.EnableVignette) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    // Lens
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Lens Layer", &m_fxState.EnableLens);

        if (!m_fxState.EnableLens) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::SliderFloat("Fisheye", &vignetteParams.glitchStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Chroma Blur", &vignetteParams.blurStrength, 0.0f, 0.05f, "%.4f");
        if (!m_fxState.EnableLens) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    // CRT
    if (ImGui::CollapsingHeader("CRT Monitor Effect"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: CRT Layer", &m_fxState.EnableCRT);

        if (!m_fxState.EnableCRT) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::SliderFloat("Density", &vignetteParams.fineDensity, 10.0f, 500.0f);
        ImGui::SliderFloat("Opacity ", &vignetteParams.fineOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Speed", &vignetteParams.scanlineSpeed, -10.0f, 10.0f);
        ImGui::SliderFloat("Size", &vignetteParams.scanlineSize, 1.0f, 150.0f);
        ImGui::SliderFloat("Scanline Opacity", &vignetteParams.scanlineStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("CRT Distortion", &vignetteParams.distortion, -0.5f, 0.5f);
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