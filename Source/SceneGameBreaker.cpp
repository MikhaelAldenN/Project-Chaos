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


    m_camScenarioPoints.clear();

    // SHOT 1: Intro (Static shot, jadi Start & End sama)
    m_camScenarioPoints.push_back({
        "Shot 1 (Intro)",
        {0.0f, 20.0f, 0.0f}, {0.0f, 0.0f, 0.0f},    // START State
        {0.0f, 14.5f, 0.0f}, {-0.5f, 0.0f, 1.5f},   // END State (Sama karena diam)
        3.0f,
        EasingType::EaseInCubic
        });

    // SHOT 2: Angle B (Bergerak sedikit/Fly through)
    m_camScenarioPoints.push_back({
        "Shot 2 (Angle B)",
        {1.9f, 12.3f, 6.8f}, {0.0f, 0.0f, 2.1f},    // START
        {-0.4f, 8.5f, 9.3f}, {-0.5f, 0.0f, 1.4f},   // END
        3.0f,
        EasingType::EaseOutCubic // Pakai Linear/SmoothStep biar kelihatan geraknya
        });

    // SHOT 3: Action
    //m_camScenarioPoints.push_back({
    //    "Shot 3 (Action)",
    //    {1.9f, 12.3f, 6.8f}, {0.0f, 0.0f, 2.1f}, // START
    //    {0.0f, 8.5f, 9.3f}, {-0.1f, 0.0f, 1.4f}, // END
    //    1.5f,
    //    EasingType::Linear
    //    });

    // --------------------------------------------------------
    // Initialize Assets
    // --------------------------------------------------------
    m_spriteBorderBreaker = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathBorderBreaker);
	//m_spriteDEBUG_LAYOUT = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathDebugLayout);

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
        blockManager->Update(elapsedTime, activeCam, player);
        if (ball && ball->IsActive()) blockManager->CheckCollision(ball);
    }

    // --------------------------------------------------------
    // Update Player
    // --------------------------------------------------------
    if (player)
    {
        player->Update(elapsedTime, activeCam);

        float energy = player->GetShakeEnergy();
        float thresholdForm = player->breakoutSettings.thresholdFormation;
        float thresholdDest = player->breakoutSettings.thresholdDestruction;

        // PHASE 1: Formation Mode
        if (energy >= thresholdForm && blockManager && !blockManager->IsFormationActive())
        {
            blockManager->ActivateFormationMode();
            player->SetGameStage(1); // Lock checkpoint
        }

        // PHASE 2: Destruction Mode
        if (energy >= thresholdDest)
        {
            if (paddle && paddle->IsActive()) paddle->SetActive(false);
            if (ball) ball->SetBoundariesEnabled(false);

            player->SetGameStage(2); // Lock final checkpoint
        }
    }

    CameraController::Instance().Update(elapsedTime);
    UpdateGameTriggers(elapsedTime);

    // =========================================================
    // LOGIKA FX: AUTO GLITCH TRANSITION
    // =========================================================
    // Kita cek waktu sequence kamera
    auto seqInfo = CameraController::Instance().GetSequenceProgress();

    static bool wasPlayingSequence = false;

    if (seqInfo.IsPlaying && m_fxState.MasterEnabled && m_fxState.EnableLens)
    {
        wasPlayingSequence = true;

        float t = seqInfo.CurrentTime;
        float d = seqInfo.TotalDuration;
        float remaining = d - t;
        float transitionWindow = 0.2f;

        // Cek apakah ini Shot Terakhir?
        bool isLastShot = (seqInfo.CurrentIndex == seqInfo.TotalShots - 1);

        // PHASE 1: PRE-CUT (Menjelang pindah)
        // SYARAT BARU: && !isLastShot
        // Jangan glitch kalau ini shot terakhir, biarkan bersih sampai selesai.
        if (remaining <= transitionWindow && d > 0.0f && !isLastShot)
        {
            float ratio = 1.0f - (remaining / transitionWindow);
            uberParams.glitchStrength = ratio;
        }
        // PHASE 2: POST-CUT (Baru saja pindah)
        else if (t <= transitionWindow && seqInfo.CurrentIndex > 0)
        {
            float ratio = t / transitionWindow;
            uberParams.glitchStrength = 0.7f * (1.0f - ratio);
        }
        // NORMAL STATE
        else
        {
            uberParams.glitchStrength = 0.0f;
        }
    }
    else
    {
        // LOGIKA RESET (JALAN HANYA SEKALI SAAT SEQUENCE BERHENTI)
        if (wasPlayingSequence)
        {
            // Sequence baru saja mati frame ini, paksa glitch jadi 0
            if (m_fxState.MasterEnabled && m_fxState.EnableLens)
            {
                uberParams.glitchStrength = 0.0f;
            }
            wasPlayingSequence = false; // Reset flag biar gak ganggu manual slider
        }
    }
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
        PlayCinematicTrigger();
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

    // --- C. Text ---
    // [NEW] Render the tutorial text here so it's part of the scene (gets post-processed)
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (text)
    {
        // Setup Blend State for Text
        dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);

        text->Draw(tutorialText.c_str(),
            tutorialLayout.x, tutorialLayout.y, tutorialLayout.scale,
            tutorialLayout.color[0], tutorialLayout.color[1], tutorialLayout.color[2], tutorialLayout.color[3]
        );
    }

    // --- D. Debug Shapes ---
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
    if (paddle && paddle->IsActive()) { modelRenderer->Draw(ShaderId::Phong, paddle->GetModel(), paddle->color); }
    if (player) { modelRenderer->Draw(ShaderId::Phong, player->GetModel(), player->color); }

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

void SceneGameBreaker::ImGuiEditPanel(UI_LayoutData& layout)
{
    if (ImGui::TreeNode(layout.name))
    {
        ImGui::DragFloat("Pos X", &layout.x, 1.0f, 0.0f, 1920.0f);
        ImGui::DragFloat("Pos Y", &layout.y, 1.0f, 0.0f, 1080.0f);
        ImGui::DragFloat("Scale", &layout.scale, 0.01f, 0.1f, 5.0f);
        
        ImGui::DragFloat("Line Spacing", &layout.lineSpacing, 1.0f, 0.0f, 200.0f);
        ImGui::ColorEdit4("Color", layout.color);

        ImGui::TreePop();
    }
}

// SceneGameBreaker.cpp GUI
void SceneGameBreaker::DrawGUI()
{
    // 1. Debug Controller (Window Terpisah)
    CameraController::Instance().DrawDebugGUI();

    // 2. Window Utama "Scene Inspector"
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    // KITA BUKA WINDOW DI SINI
    if (ImGui::Begin("Scene Inspector", nullptr))
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

            // TAB 2: UI LAYOUT
            if (ImGui::BeginTabItem("UI Layout"))
            {
                ImGui::Spacing();
                ImGui::Text("Adjust In-Game UI Elements:");
                ImGui::Separator();

                // Drop down Tutorial Setting
                ImGuiEditPanel(tutorialLayout);

                ImGui::EndTabItem();
            }

            // --- TAB 3: POST PROCESS ---
            if (ImGui::BeginTabItem("Post-Process & FX"))
            {
                GUIPostProcessTab();
                ImGui::EndTabItem();
            }

            // --- TAB: OBJECT COLOR ---
            if (ImGui::BeginTabItem("Object Color"))
            {
                ImGui::Spacing();

                // Header "Character"
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CHARACTER");
                ImGui::Separator();
                ImGui::Spacing();

                // 1. Drop down "Paddle"
                if (paddle)
                {
                    if (ImGui::CollapsingHeader("Paddle", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Indent();
                        ImGui::ColorEdit4("Base Color##Paddle", &paddle->color.x);
                        ImGui::Unindent();
                    }
                }

                // 2. Drop down "Blocks"
                if (blockManager)
                {
                    if (ImGui::CollapsingHeader("Blocks"))
                    {
                        ImGui::Indent();
                        // This controls the 'globalBlockColor' we added to BlockManager
                        ImGui::ColorEdit4("Base Color##Blocks", &blockManager->globalBlockColor.x);
                        ImGui::Unindent();
                    }
                }

                // 3. Drop down "Player"
                if (player)
                {
                    if (ImGui::CollapsingHeader("Player"))
                    {
                        ImGui::Indent();
                        ImGui::ColorEdit4("Base Color##Player", &player->color.x);
                        ImGui::Unindent();
                    }
                }

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
    auto mainCam = GetMainCamera();

    // --- STATIC STATES (Untuk GUI) ---
    static bool isSequencePlaying = false;
    static int selectedMode = 0; // 0 = Jump Cut (Hard), 1 = Smooth Travel (Spline)
    static float travelSpeed = 5.0f; // Hanya untuk Smooth Mode
    static float curveTension = 0.5f;

    // Styling Helper
    auto HelpMarker = [](const char* desc) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        };

    // =========================================================
    // 1. TRANSPORT CONTROL (PLAY / STOP)
    // =========================================================
    ImGui::Spacing();
    float availWidth = ImGui::GetContentRegionAvail().x;

    // Cek status controller asli untuk sync tombol
    bool ctrlIsSequencing = camCtrl.IsSequencing();
    if (!ctrlIsSequencing) isSequencePlaying = false;

    if (isSequencePlaying)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Merah
        if (ImGui::Button("STOP SEQUENCE", ImVec2(availWidth, 40)))
        {
            camCtrl.StopSequence();
            isSequencePlaying = false;

            // Reset ke Shot 1 (Start Point)
            if (!m_camScenarioPoints.empty()) {
                // PERBAIKAN: Gunakan StartPos dan StartLookAt
                camCtrl.SetFixedSetting(m_camScenarioPoints[0].StartPos);
                camCtrl.SetTarget(m_camScenarioPoints[0].StartLookAt);
            }
        }
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Hijau
        if (ImGui::Button("PLAY SEQUENCE", ImVec2(availWidth, 40)))
        {
            std::vector<CameraKeyframe> sequence;
            for (const auto& point : m_camScenarioPoints)
            {
                CameraKeyframe key;

                // SETTING JUMP CUT / CUT & PAN
                key.isJumpCut = true; // Kita aktifkan mode ini sesuai requestmu

                // 1. Tentukan Titik Awal (Start)
                key.StartPosition = point.StartPos;
                key.StartRotation = CalculateRotationFromTarget(point.StartPos, point.StartLookAt);

                // 2. Tentukan Titik Akhir (Target)
                key.TargetPosition = point.EndPos;
                key.TargetRotation = CalculateRotationFromTarget(point.EndPos, point.EndLookAt);

                key.Duration = point.Duration;
                key.Easing = point.Easing; // Misal: Linear atau SmoothStep

                sequence.push_back(key);
            }
            // Play!
            camCtrl.PlaySequence(sequence, false);
            isSequencePlaying = true;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // =========================================================
    // 2. GLOBAL SETTINGS
    // =========================================================
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "SEQUENCE SETTINGS");

    const char* modes[] = { "Jump Cut (Cinematic)", "Smooth Travel (Fly-through)" };
    ImGui::Combo("Transition Mode", &selectedMode, modes, IM_ARRAYSIZE(modes));

    if (selectedMode == 1) // Hanya tampil jika Smooth Mode
    {
        ImGui::Indent();
        ImGui::DragFloat("Travel Speed", &travelSpeed, 0.5f, 0.1f, 100.0f, "%.1f m/s");
        ImGui::SliderFloat("Curve Tension", &curveTension, 0.0f, 2.0f);
        HelpMarker("0.0 = Straight Lines\n0.5 = Natural Curve\n1.0+ = Wide Turns");
        ImGui::Unindent();
    }
    else
    {
        ImGui::Indent();
        ImGui::TextDisabled("Mode: Instant switching between shots.");
        ImGui::TextDisabled("Duration controls 'Hold Time' per shot.");
        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================
    // 3. SHOT LIST (TIMELINE)
    // =========================================================
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "SHOT LIST / KEYFRAMES");

    // Loop semua point
    for (int i = 0; i < m_camScenarioPoints.size(); ++i)
    {
        auto& pt = m_camScenarioPoints[i];
        ImGui::PushID(i);

        char headerName[64];
        snprintf(headerName, 64, "#%d - %s", i + 1, pt.Name.c_str());

        if (ImGui::CollapsingHeader(headerName, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            // --- TIMING ---
            ImGui::DragFloat("Duration", &pt.Duration, 0.1f, 0.1f, 20.0f);

            // Easing selector (Opsional, Linear biasanya bagus untuk panning)
            // --- MOTION TYPE SELECTOR ---
            // Kita mapping manual dari Enum ke Menu Index biar aman & rapi
            int currentEaseIdx = 0;
            if (pt.Easing == EasingType::Linear)        currentEaseIdx = 0;
            else if (pt.Easing == EasingType::EaseInCubic)   currentEaseIdx = 1; // Ease In
            else if (pt.Easing == EasingType::EaseOutCubic)  currentEaseIdx = 2; // Ease Out
            else if (pt.Easing == EasingType::SmoothStep)    currentEaseIdx = 3; // Ease In Out

            const char* easeItems[] = {
                "Linear (Constant)",
                "Ease In (Start Slow)",
                "Ease Out (Stop Slow)",
                "Smooth Step (Ease In-Out)"
            };

            if (ImGui::Combo("Motion Type", &currentEaseIdx, easeItems, IM_ARRAYSIZE(easeItems)))
            {
                switch (currentEaseIdx)
                {
                case 0: pt.Easing = EasingType::Linear; break;
                case 1: pt.Easing = EasingType::EaseInCubic; break; // Pakai Cubic biar efeknya kerasa
                case 2: pt.Easing = EasingType::EaseOutCubic; break;
                case 3: pt.Easing = EasingType::SmoothStep; break;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            // KITA BUAT 2 KOLOM: START vs END
            ImGui::Columns(2, nullptr, true); // Border true biar jelas

            // === KOLOM KIRI: START ===
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ A ] START POINT");

            if (ImGui::Button("View Start")) {
                // Teleport kamera ke posisi Start untuk preview
                camCtrl.StopSequence();
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                camCtrl.SetFixedSetting(pt.StartPos);
                camCtrl.SetTarget(pt.StartLookAt);
                if (mainCam) { mainCam->SetPosition(pt.StartPos); mainCam->LookAt(pt.StartLookAt); }
            }

            // TOMBOL CAPTURE UNTUK START
            if (ImGui::Button("Set to Current Cam##Start")) {
                if (mainCam) {
                    pt.StartPos = mainCam->GetPosition();
                    // Hitung LookAt target dari rotasi
                    XMFLOAT3 rot = mainCam->GetRotation();
                    XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
                    XMStoreFloat3(&pt.StartLookAt, XMVectorAdd(XMLoadFloat3(&pt.StartPos), vDir));
                }
            }

            // Manual Edit Start (Hidden by default)
            if (ImGui::TreeNode("Edit Start Coords")) {
                // Kita pakai variabel 'changed' untuk mendeteksi geseran mouse
                bool changed = false;
                changed |= ImGui::DragFloat3("Pos##S", &pt.StartPos.x, 0.1f);
                changed |= ImGui::DragFloat3("Look##S", &pt.StartLookAt.x, 0.1f);

                // Jika ada perubahan angka, LANGSUNG paksa kamera pindah
                if (changed) {
                    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                    camCtrl.SetFixedSetting(pt.StartPos);
                    camCtrl.SetTarget(pt.StartLookAt);

                    // Update visual langsung biar gak nunggu frame berikutnya
                    if (mainCam) {
                        mainCam->SetPosition(pt.StartPos);
                        mainCam->LookAt(pt.StartLookAt);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::NextColumn();

            // === KOLOM KANAN: END ===
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[ B ] END POINT");

            if (ImGui::Button("View End")) {
                // Teleport kamera ke posisi End untuk preview
                camCtrl.StopSequence();
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                camCtrl.SetFixedSetting(pt.EndPos);
                camCtrl.SetTarget(pt.EndLookAt);
                if (mainCam) { mainCam->SetPosition(pt.EndPos); mainCam->LookAt(pt.EndLookAt); }
            }

            // TOMBOL CAPTURE UNTUK END
            if (ImGui::Button("Set to Current Cam##End")) {
                if (mainCam) {
                    pt.EndPos = mainCam->GetPosition();
                    XMFLOAT3 rot = mainCam->GetRotation();
                    XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
                    XMStoreFloat3(&pt.EndLookAt, XMVectorAdd(XMLoadFloat3(&pt.EndPos), vDir));
                }
            }

            // Manual Edit End
            if (ImGui::TreeNode("Edit End Coords")) {
                bool changed = false;
                changed |= ImGui::DragFloat3("Pos##E", &pt.EndPos.x, 0.1f);
                changed |= ImGui::DragFloat3("Look##E", &pt.EndLookAt.x, 0.1f);

                // Logic yang sama untuk End Point
                if (changed) {
                    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                    camCtrl.SetFixedSetting(pt.EndPos);
                    camCtrl.SetTarget(pt.EndLookAt);

                    if (mainCam) {
                        mainCam->SetPosition(pt.EndPos);
                        mainCam->LookAt(pt.EndLookAt);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::Columns(1); // Balik ke 1 kolom
            ImGui::Unindent();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button("+ Add New Shot at Current View", ImVec2(-1, 30)))
    {
        SceneCameraPoint newPt;
        newPt.Name = "New Shot";
        newPt.Duration = 2.0f;
        newPt.Easing = EasingType::Step;

        if (mainCam) {
            // Ambil posisi kamera saat ini
            XMFLOAT3 currPos = mainCam->GetPosition();

            // Hitung target lookat
            XMFLOAT3 rot = mainCam->GetRotation();
            XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
            XMFLOAT3 currLookAt;
            XMStoreFloat3(&currLookAt, XMVectorAdd(XMLoadFloat3(&currPos), vDir));

            // PERBAIKAN: Isi Start DAN End dengan posisi yang sama (sebagai default)
            newPt.StartPos = currPos;
            newPt.StartLookAt = currLookAt;

            newPt.EndPos = currPos;
            newPt.EndLookAt = currLookAt;
        }
        else {
            // Default value jika tidak ada kamera
            newPt.StartPos = { 0, 5, 0 }; newPt.StartLookAt = { 0, 0, 0 };
            newPt.EndPos = { 0, 5, 0 };   newPt.EndLookAt = { 0, 0, 0 };
        }
        m_camScenarioPoints.push_back(newPt);
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

void SceneGameBreaker::PlayCinematicTrigger()
{
    if (m_camScenarioPoints.empty()) return;

    std::vector<CameraKeyframe> sequence;

    for (const auto& point : m_camScenarioPoints)
    {
        CameraKeyframe key;

        // Mode Cinematic: Jump Cut aktif
        key.isJumpCut = true;

        // 1. Start State
        key.StartPosition = point.StartPos;
        key.StartRotation = CalculateRotationFromTarget(point.StartPos, point.StartLookAt);

        // 2. End State
        key.TargetPosition = point.EndPos;
        key.TargetRotation = CalculateRotationFromTarget(point.EndPos, point.EndLookAt);

        // 3. Timing
        key.Duration = point.Duration;
        key.Easing = point.Easing;

        sequence.push_back(key);
    }

    // Eksekusi sequence
    CameraController::Instance().PlaySequence(sequence, false);
}