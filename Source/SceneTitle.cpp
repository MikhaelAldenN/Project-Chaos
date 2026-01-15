#include "SceneTitle.h"
#include "SceneGameBreaker.h"
#include "Framework.h"
#include "System/Input.h"
#include "System/Graphics.h"
#include <imgui.h>
#include "ResourceManager.h"

SceneTitle::SceneTitle()
{
    // 1. Setup Camera (Orthographic Fixed 1920x1080)
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->SetRotation(0.0f, 0.0f, 0.0f);

    primitiveBatcher = std::make_unique<Primitive>(Graphics::Instance().GetDevice());
    uiManager = std::make_unique<ButtonManager>();
    auto btnExit = std::make_unique<UIButtonPrimitive>(
        primitiveBatcher.get(),
        "Quit.exe",
        50.0f, 600.0f, 200.0f, 40.0f
    );

    debugBtnExit = btnExit.get();

    // Style warna custom (Opsional) - Misal tema Merah Error
    btnExit->SetColors({ 0.5f, 0.0f, 0.0f }, { 0.8f, 0.0f, 0.0f }, { 0.3f, 0.0f, 0.0f });

    btnExit->SetOnClick([]() {  });

    uiManager->AddButton(std::move(btnExit));

    // 2. Load Resources
    bgSprite = std::make_unique<Sprite>(
        Graphics::Instance().GetDevice(),
        "Data/Sprite/Scene Title/Sprite_BorderBrickDos.png"
    );

    // 3. Initialize Data & Layout
    SetupLayout();
    SetupContent();

    // Initialize UberShader & Render Targets
    uberShader = std::make_unique<UberShader>(Graphics::Instance().GetDevice());
    CreateRenderTarget();
}

void SceneTitle::SetupLayout()
{
    // Default Values (Bisa ditweak lewat ImGui nanti)

    // Panel Status (Atas)
    statusPanel = { "Status Panel", 55.0f, 105.0f, 0.0f, 0.625f, {0.0f, 1.0f, 1.0f, 1.0f} }; // Cyan

    // Panel Directory (Tengah)
    dirPanel = { "Directory Panel", 55.0f, 320.0f, 40.0f, 0.625f, {1.0f, 1.0f, 1.0f, 1.0f} }; // White

    // Panel Logs (Bawah)
    logPanel = { "System Logs", 55.0f, 680.0f, 35.0f, 0.625f, {0.7f, 0.7f, 0.7f, 1.0f} }; // Greyish
}

void SceneTitle::SetupContent()
{
    // Status
    statusText = "Online - UNSTABLE";

    // Directory
    directoryFiles = {
        "/..",
        "BeyondBreak.exe   3256",
        "README.txt         275",
        "settings.ini        83",
        "Misc/            <DIR>",
        "Exit.exe          4096"
    };

    // System Logs
    systemLogs = {
        "BOOT: Loading kernel",
        "v1.2... OK",
        "MEM: 640KB RAM Check... OK",
        "DRV: Mounting IDE Sector",
        "", // Empty line spacing
        "WRN: Inconsistency",
        "detected in sector 0x004"
    };
}

void SceneTitle::Update(float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > 1000.0f) m_globalTime -= 1000.0f;

    uiManager->Update();

    // Scene Switching
    if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
    {
        Framework::Instance()->ChangeScene(std::make_unique<SceneGameBreaker>());
    }
}

void SceneTitle::DrawListInPanel(const std::vector<std::string>& list, const PanelLayout& layout)
{
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (!text) return;

    float currentY = layout.y;
    for (const auto& line : list)
    {
        text->Draw(line.c_str(),
            layout.x, currentY, layout.scale,
            layout.color[0], layout.color[1], layout.color[2], layout.color[3]);

        currentY += layout.lineSpacing;
    }
}

void SceneTitle::Render(float dt, Camera* targetCamera)
{
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

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
    // Setup Render State untuk 2D/Sprite
    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

    // 1. Render Background
    if (bgSprite)
    {
        bgSprite->Render(
            dc, camera.get(),
            0, 0, 0,        // Center
            1920.0f, 1080.0f,
            0.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        );
    }
    
    uiManager->Render(Graphics::Instance().GetDeviceContext(), targetCamera);


    // 2. Render Text UI
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (text)
    {
        // Render Status (Manual call karena cuma 1 baris)
        text->Draw(statusText.c_str(),
            statusPanel.x, statusPanel.y, statusPanel.scale,
            statusPanel.color[0], statusPanel.color[1], statusPanel.color[2], statusPanel.color[3]);

        // Render Lists
        DrawListInPanel(directoryFiles, dirPanel);
        DrawListInPanel(systemLogs, logPanel);
    }

    primitiveBatcher->Render(Graphics::Instance().GetDeviceContext());

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

void SceneTitle::OnResize(int width, int height)
{
    // Recreate buffer on resize
    CreateRenderTarget();
}

// --- IMGUI IMPLEMENTATION ---

void SceneTitle::ImGuiEditPanel(PanelLayout& layout)
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

void SceneTitle::DrawGUI()
{
    ImGui::Begin("Title Scene Debugger");

    if (ImGui::BeginTabBar("InspectorTabs"))
    {
        // TAB 1: UI & Layout (Existing)
        if (ImGui::BeginTabItem("UI Layout"))
        {
            if (ImGui::CollapsingHeader("Layout Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Tweak UI positions here:");
                ImGui::Separator();

                ImGuiEditPanel(statusPanel);
                ImGuiEditPanel(dirPanel);
                ImGuiEditPanel(logPanel);
            }

            if (ImGui::CollapsingHeader("Button Inspector", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (debugBtnExit)
                {
                    ImGui::Text("Target: BeyondBreaker.exe");

                    float pos[2] = { debugBtnExit->GetX(), debugBtnExit->GetY() };
                    float size[2] = { debugBtnExit->GetWidth(), debugBtnExit->GetHeight() };

                    if (ImGui::DragFloat2("Position", pos)) debugBtnExit->SetPosition(pos[0], pos[1]);
                    if (ImGui::DragFloat2("Size", size, 1.0f, 10.0f, 1000.0f)) debugBtnExit->SetSize(size[0], size[1]);

                    ImGui::DragFloat("Text Scale", &debugBtnExit->GetTextScale(), 0.01f, 0.1f, 5.0f);

                    ImGui::Separator();
                    ImGui::Text("Text Layout");
                    ImGui::DragFloat("Padding X", &debugBtnExit->GetPadding(), 1.0f, 0.0f, 500.0f);
                    ImGui::DragFloat("V-Adjust (Y)", &debugBtnExit->GetVerticalAdjustment(), 0.5f, -50.0f, 50.0f);

                    int alignState = (int)debugBtnExit->GetAlignment();
                    const char* items[] = { "Center", "Left", "Right" };
                    if (ImGui::Combo("Alignment", &alignState, items, IM_ARRAYSIZE(items)))
                    {
                        debugBtnExit->SetAlignment((TextAlignment)alignState);
                    }

                    ImGui::Separator();
                    ImGui::Text("Color Pallete (RGB)");
                    ImGui::ColorEdit3("Normal", &debugBtnExit->GetColorNormal().x);
                    ImGui::ColorEdit3("Hover", &debugBtnExit->GetColorHover().x);
                    ImGui::ColorEdit3("Pressed", &debugBtnExit->GetColorPress().x);
                    ImGui::ColorEdit3("Border", &debugBtnExit->GetColorBorder().x);
                }
            }
            ImGui::EndTabItem();
        }

        // TAB 2: Post-Processing (NEW)
        if (ImGui::BeginTabItem("Post-Process & FX"))
        {
            GUIPostProcessTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void SceneTitle::GUIPostProcessTab()
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

void SceneTitle::CreateRenderTarget()
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