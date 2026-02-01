#include <imgui.h>
#include "SceneIntroBios.h"
#include "SceneIntroOS.h"
#include "Framework.h"
#include "System/Input.h"
#include "System/Graphics.h"

// --- KONFIGURASI BIOS ---
namespace ConfigBios {
    constexpr float FONT_SIZE = 0.625f;
    const float COLOR_YELLOW[4] = { 0.96f, 0.80f, 0.23f, 1.0f };
    const float COLOR_RED[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

    constexpr float FINISH_DELAY = 0.5f;
    constexpr float EXIT_DELAY = 1.5f;
}

SceneIntroBios::SceneIntroBios()
{
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);

    // Load Logo
    spriteLogoBoot = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Intro/Sprite_OrangSoloItu.png");

    SetupBiosLog();

    // Initialize UberShader & Render Targets
    uberShader = std::make_unique<UberShader>(Graphics::Instance().GetDevice());
    CreateRenderTarget();     

    AudioManager::Instance().PlayMusic("Data/Sound/BGM_Intro.wav", true, 6.0f);
}

void SceneIntroBios::SetupBiosLog()
{
    biosLogSystem = std::make_unique<Typewriter>();
    using namespace ConfigBios;

    biosLogSystem->AddLine(
        "SURAKARTAN - SYSTEM BOOT LOG \n\nBEYOND Modular BIOS v9.70BB, An Energy Star Ally \nCopyright (C) 1987-2026, Software Chaos, Inc. ",
        406.5f, 57.4f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "Version YN7K75\nNEURAL-LINK CPU at 444MHz",
        298.0f, 175.74f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddMemoryTestLine(
        "Memory Test : ", 0, 262144, 2.0f, "KB OK",
        740.0f, 175.74f + 23.5f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "(Reference: \"640K ought to be enough for anybody\")",
        298.0f, 235.0f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "Chaos Plug and Play BIOS Extension v1.0A\nCopyright (C) 2026, Software Chaos, Inc.",
        298.0f, 296.79f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine("IDE Primary Master    ... [ST320411A]", 334.7f, 344.31f, FONT_SIZE, COLOR_YELLOW, 1.0f);
    biosLogSystem->AddLine("IDE Primary Slave     ... NONE", 334.7f, 368.5f, FONT_SIZE, COLOR_YELLOW);
    biosLogSystem->AddLine("IDE Secondary Master  ... [ECC DVD-ROM]\nIDE Secondary Slave   ... [!] WARNING: ANOMALY_DETECTED", 334.7f, 392.5f, FONT_SIZE, COLOR_YELLOW);

    biosLogSystem->AddLine(
        "[CRITICAL ALERT] Sector 0x004 is bleeding.\nThe blocks are no longer static. The Wall is thinner than you think.",
        298.7f, 560.0f, FONT_SIZE, COLOR_YELLOW
    );
}

void SceneIntroBios::Update(float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > 1000.0f) m_globalTime -= 1000.0f;

    if (isExiting)
    {
        exitTimer += elapsedTime;
        if (exitTimer >= ConfigBios::EXIT_DELAY)
        {
            Framework::Instance()->ChangeScene(std::make_unique<SceneIntroOS>());
        }
        return;
    }

    if (biosLogSystem)
    {
        biosLogSystem->Update(elapsedTime);
        if (biosLogSystem->IsFinished())
        {
            finishDelayTimer += elapsedTime;
            if (finishDelayTimer >= ConfigBios::FINISH_DELAY)
            {
                isExiting = true;
            }
        }
    }

    if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
    {
        if (biosLogSystem && !biosLogSystem->IsFinished())
        {
            biosLogSystem->SkipCurrentLine();
        }
        else
        {
            isExiting = true;
        }
    }
}

void SceneIntroBios::Render(float dt, Camera* targetCamera)
{
    if (isExiting) return;

    auto dc = Graphics::Instance().GetDeviceContext();

    // =========================================================
    // SAVE BACK BUFFER 
    // =========================================================
    ID3D11RenderTargetView* originalRTV = nullptr;
    ID3D11DepthStencilView* originalDSV = nullptr;
    dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);

    // =========================================================
    // DECIDE RENDER TARGET
    // =========================================================
    // Check GUI Switch
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

    // Bind the chosen target
    dc->OMSetRenderTargets(1, &currentRTV, currentDSV);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    dc->ClearRenderTargetView(currentRTV, clearColor);
    dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // =========================================================
    // RENDER SCENE CONTENT
    // =========================================================
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), blendFactor, 0xFFFFFFFF);
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");

    if (spriteLogoBoot)
    {
        spriteLogoBoot->Render(dc,
            298.0f, 57.4f, 0.0f,    // X, Y, Z
            93.0f, 93.0f,           // W, H
            0.0f,                   // Angle
            1.0f, 1.0f, 1.0f, 1.0f  // Color
        );
    }

    if (text)
    {
        text->Draw(
            "Press DEL to enter SETUP Press F1 to ESCAPE THE GRID\n09/01/2026-BB-586B-W877-2A5LEF09C-00",
            298.7f, 973.5f,
            ConfigBios::FONT_SIZE,
            ConfigBios::COLOR_YELLOW[0], ConfigBios::COLOR_YELLOW[1], ConfigBios::COLOR_YELLOW[2], ConfigBios::COLOR_YELLOW[3]
        );
    }

    if (biosLogSystem && text)
    {
        biosLogSystem->Render(text);
    }

    // =========================================================
    // APPLY POST-PROCESSING 
    // =========================================================
    if (usePostProcess)
    {
        // Restore Back Buffer
        dc->OMSetRenderTargets(1, &originalRTV, originalDSV);

        // Setup States
        auto rs = Graphics::Instance().GetRenderState();
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

        // Copy Params
        UberShader::UberData finalParams = uberParams;
        finalParams.time = m_globalTime;

        // --- APPLY GUI OVERRIDES ---
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

        // Draw
        uberShader->Draw(dc, shaderResourceView.Get(), finalParams);

        // Cleanup
        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        dc->PSSetShaderResources(0, 1, nullSRV);
    }

    if (originalRTV) originalRTV->Release();
    if (originalDSV) originalDSV->Release();
}

void SceneIntroBios::OnResize(int width, int height)
{
    if (camera && height > 0)
    {
        camera->SetAspectRatio((float)width / (float)height);
    }

    CreateRenderTarget();
}

void SceneIntroBios::DrawGUI() 
{
    //ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    //if (ImGui::Begin("Bios Inspector", nullptr))
    //{
    //    if (ImGui::BeginTabBar("InspectorTabs"))
    //    {
    //        if (ImGui::BeginTabItem("Post-Process & FX"))
    //        {
    //            GUIPostProcessTab();
    //            ImGui::EndTabItem();
    //        }
    //        ImGui::EndTabBar();
    //    }
    //}
    //ImGui::End();
}

void SceneIntroBios::GUIPostProcessTab()
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

void SceneIntroBios::CreateRenderTarget()
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
    device->CreateRenderTargetView(renderTargetTexture.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(renderTargetTexture.Get(), nullptr, shaderResourceView.ReleaseAndGetAddressOf());

    // Create Depth Buffer
    textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&textureDesc, nullptr, depthStencilTexture.ReleaseAndGetAddressOf());
    device->CreateDepthStencilView(depthStencilTexture.Get(), nullptr, depthStencilView.ReleaseAndGetAddressOf());
}