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

    // =========================================================
    // 1. SETUP DEFAULT THEME (Di dalam MenuConfig)
    // =========================================================

        // Style: Standby (Transparan + Teks Kuning)
    menuConfig.styleStandby.backColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    menuConfig.styleStandby.borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    menuConfig.styleStandby.textColor = { 0.96f, 0.80f, 0.23f, 1.0f };

    // Style: Hover (Biru DOS + Teks Putih)
    menuConfig.styleHover.backColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.styleHover.borderColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.styleHover.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Style: Pressed (Putih + Teks Biru)
    menuConfig.stylePress.backColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.stylePress.borderColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.stylePress.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // =========================================================
    // A. TOMBOL DEKORASI INDEPENDEN ("/..")
    // =========================================================
    // Posisi Y: 280.0f (Di atas tombol pertama yang Y-nya 320.0f)
    auto btnUpDir = std::make_unique<UIButtonPrimitive>(
        primitiveBatcher.get(), " /..            <UP DIR>",
        menuConfig.startX, 260.0f, // Posisi Manual
        menuConfig.btnWidth, menuConfig.btnHeight
    );

    // Styling Manual (Menggunakan style dari config biar seragam)
    // Trik: Set style PRESSED sama dengan HOVER atau PRESS biasa, 
    // tapi karena tidak ada logic 'SetSelected', dia hanya kedip sebentar saat diklik.
    btnUpDir->SetStyle(ButtonState::STANDBY, menuConfig.styleStandby);
    btnUpDir->SetStyle(ButtonState::HOVER, menuConfig.styleHover);
    btnUpDir->SetStyle(ButtonState::PRESSED, menuConfig.stylePress);

    // Layout Manual
    btnUpDir->SetPadding(8.0f);
    btnUpDir->SetAlignment(TextAlignment::Left); // Atau Left sesuai selera
    btnUpDir->SetTextScale(menuConfig.textScale);
    btnUpDir->SetVerticalAdjustment(menuConfig.verticalAdj);

    // Callback SEDERHANA (Tanpa Logika Radio Button/Selected)
    btnUpDir->SetOnClick([]() {
        printf("Clicked decoration /..\n");
        // TIDAK ADA: currentActiveButton->SetSelected(true); 
        // Jadi dia tidak akan nyangkut warnanya.
        });

    // Masukkan langsung ke Manager (Tidak perlu masuk debugButtonList)
    uiManager->AddButton(std::move(btnUpDir));

    // =========================================================
    // 2. GENERATE BUTTONS
    // =========================================================
    std::vector<std::string> fileList = {
        "BeyondBreak...    3256", 
        "README.txt         275", 
        "settings.ini        83", 
        "Misc/            <DIR>", 
        "Exit.exe          4096"
    };

    debugButtonList.clear();

    for (int i = 0; i < fileList.size(); ++i)
    {
        // Buat button (Posisi 0,0 dulu, nanti diatur ApplyMenuLayout)
        auto btn = std::make_unique<UIButtonPrimitive>(
            primitiveBatcher.get(), fileList[i], 0, 0, 100, 20
        );

        // Callback (Logic klik)
        std::string fileName = fileList[i]; // Key untuk Map
        UIButtonPrimitive* rawBtnPtr = btn.get();

        btn->SetOnClick([this, fileName, rawBtnPtr]() {

            // 1. Logic Radio Button (Visual)
            if (currentActiveButton && currentActiveButton != rawBtnPtr) {
                currentActiveButton->SetSelected(false);
            }
            currentActiveButton = rawBtnPtr;
            currentActiveButton->SetSelected(true);

            // 2. LOGIC KONTEN (UPDATE STATE) - INI YANG BARU
            // Kita simpan nama file yang diklik ke variabel member
            this->selectedFileName = fileName;

            // 3. System Logic
            printf("[System] Selected: %s\n", fileName.c_str());
            if (fileName.find("Exit.exe") != std::string::npos) PostQuitMessage(0);
            });

        // ------------------------------------

        debugButtonList.push_back(btn.get());
        uiManager->AddButton(std::move(btn));
    }

    if (!debugButtonList.empty()) debugBtnExit = debugButtonList.back();

    // Terapkan Layout & Warna sekaligus
    ApplyMenuLayout();

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
    panelStatus = { "Status Panel", 341.0f, 132.0f, 0.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} }; // Cyan

    // Panel Directory (Tengah)
    panelDirectory = { "Directory Panel",  395.0f, 234.0f, 40.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} }; // White

    // Panel Logs (Bawah)
    //logPanel = { "System Logs", 55.0f, 680.0f, 35.0f, 0.625f, {0.7f, 0.7f, 0.7f, 1.0f} }; // Greyish

    panelDescription = { "Desc Panel", 827.0f, 132.0f, 30.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
}

void SceneTitle::SetupContent()
{
    // Status
    textStatusOnline = "Online - Unstable";
    textDirectoryHeader = "NAME             SIZE";

    fileDatabase["BeyondBreak...    3256"] = {
        "APPLICATION INFO", // Title
        {
            "App : BeyondBreaker.exe",
            "Ver : Alpha 1.0",
            "Dir : C:\\GAMES\\BEYOND_BREAKER\\BIN",
            "",
            "Desc:",
            "Project Game Programming tahun ke-2.",
            "Menghancurkan batas dimensi OS.",
            "Tekan ENTER untuk memulai."
        }
    };

    // 2. Data untuk README.txt (Credits)
    fileDatabase["README.txt         275"] = {
        "FILE CONTENT: README.TXT",
        {
            "---------------------------------",
            "        CREDITS & TEAM",
            "---------------------------------",
            "Lead Programmer : [Nama Kamu]",
            "Game Design     : [Nama Teman]",
            "Art & Sound     : Asset Store",
            "",
            "Special Thanks to:",
            "- ",
            "- ",
            "- "
        }
    };

    // 3. Data untuk Settings.ini
    fileDatabase["settings.ini        83"] = {
        "CONFIGURATION",
        {
            "[Video]",
            "Resolution=1920x1080",
            "Fullscreen=True",
            "VSync=On",
            "",
            "[Audio]",
            "MasterVolume=80",
            "BGM=100"
        }
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
        text->Draw(textStatusOnline.c_str(),
            panelStatus.x, panelStatus.y, panelStatus.scale,
            panelStatus.color[0], panelStatus.color[1], panelStatus.color[2], panelStatus.color[3]);

        // B. Render Header "Name Size" (BARU)
        // Ini adalah cara standar merender teks di engine-mu:
        text->Draw(textDirectoryHeader.c_str(),
            panelDirectory.x, panelDirectory.y, panelDirectory.scale,
            panelDirectory.color[0], panelDirectory.color[1], panelDirectory.color[2], panelDirectory.color[3]);        //DrawListInPanel(systemLogs, logPanel);
    }

    primitiveBatcher->Render(Graphics::Instance().GetDeviceContext());

    // =========================================================
    // RENDER PANEL KANAN (DYNAMIC CONTENT)
    // =========================================================
    if (text)
    {
        // Cek apakah ada file yang dipilih DAN apakah datanya ada di database?
        // .count() mengecek apakah key ada di map
        if (!selectedFileName.empty() && fileDatabase.count(selectedFileName))
        {
            const FileMetadata& data = fileDatabase[selectedFileName];

            // 1. Render Judul (Pakai warna panelDescription -> KUNING)
            text->Draw(data.title.c_str(),
                panelDescription.x, panelDescription.y,
                panelDescription.scale,
                panelDescription.color[0], panelDescription.color[1], panelDescription.color[2], panelDescription.color[3]);

            // 2. Render Isi
            float contentY = panelDescription.y + 40.0f;

            // [HAPUS] Baris ini yang bikin putih: 
            // float contentColor[4] = { 0.9f, 0.9f, 0.9f, 1.0f }; <-- HAPUS INI

            for (const std::string& line : data.lines)
            {
                // [UBAH] Parameter warna di bawah ini sekarang pakai 'panelDescription.color' juga
                text->Draw(line.c_str(),
                    panelDescription.x, contentY,
                    panelDescription.scale,
                    // Ganti contentColor[...] jadi panelDescription.color[...]
                    panelDescription.color[0], panelDescription.color[1], panelDescription.color[2], panelDescription.color[3]);

                contentY += panelDescription.lineSpacing;
            }
        }
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

void SceneTitle::OnResize(int width, int height)
{
    // Recreate buffer on resize
    CreateRenderTarget();
}

void SceneTitle::ApplyMenuLayout()
{
    for (int i = 0; i < debugButtonList.size(); ++i)
    {
        UIButtonPrimitive* btn = debugButtonList[i];

        // 1. Layout Logic
        float currentY = menuConfig.startY + (i * (menuConfig.btnHeight + menuConfig.spacing));
        btn->SetPosition(menuConfig.startX, currentY);
        btn->SetSize(menuConfig.btnWidth, menuConfig.btnHeight);
        btn->SetPadding(menuConfig.paddingX);
        btn->SetTextScale(menuConfig.textScale);
        btn->SetVerticalAdjustment(menuConfig.verticalAdj);
        btn->SetAlignment((TextAlignment)menuConfig.alignment);

        // 2. Color/Style Logic (NEW)
        // Kita copy style dari Master Config ke setiap tombol
        btn->SetStyle(ButtonState::STANDBY, menuConfig.styleStandby);
        btn->SetStyle(ButtonState::HOVER, menuConfig.styleHover);
        btn->SetStyle(ButtonState::PRESSED, menuConfig.stylePress);
    }

    if (!debugButtonList.empty())
    {
        currentActiveButton = debugButtonList[1];
        currentActiveButton->SetSelected(true);
    }
}

static void ImGuiEditButtonStyle(const char* label, ButtonStyle& style)
{
    if (ImGui::TreeNode(label))
    {
        // Preview Alpha (Transparansi) sangat penting untuk style DOS
        ImGui::ColorEdit4("Background", &style.backColor.x, ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::ColorEdit4("Border", &style.borderColor.x, ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::ColorEdit4("Text", &style.textColor.x);

        ImGui::TreePop();
    }
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
        // --------------------------------------------------------
        // TAB 1: UI LAYOUT
        // --------------------------------------------------------
        if (ImGui::BeginTabItem("UI Layout"))
        {
            // A. TEXT LAYOUT EDITOR
            if (ImGui::CollapsingHeader("Text Layouts", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();

                // 1. Status Panel (Kiri Atas)
                ImGui::PushID("p_status"); // PushID biar nama variabel internal ImGui gak bentrok
                ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "[Left Top] Status");
                ImGuiEditPanel(panelStatus);
                ImGui::PopID();

                ImGui::Separator();

                // 2. Directory Header (Kiri Tengah)
                ImGui::PushID("p_dir");
                ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "[Left Mid] Header Name/Size");
                ImGuiEditPanel(panelDirectory);
                ImGui::PopID();

                ImGui::Separator();

                // 3. DESCRIPTION PANEL (BARU - KANAN)
                // Ini yang kamu minta! Sekarang bisa geser panel kanan lewat ImGui.
                ImGui::PushID("p_desc");
                ImGui::TextColored({ 0.2f, 0.8f, 1.0f, 1.0f }, "[Right Main] Description Panel");
                ImGuiEditPanel(panelDescription);
                ImGui::PopID();

                ImGui::Unindent();
            }

            // B. MENU GROUP SETTINGS (Tombol)
            if (ImGui::CollapsingHeader("Menu Group Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // ... (Kode MenuConfig yang lama tetap sama) ...
                bool layoutChanged = false;
                // ... sliders ...
                if (layoutChanged) ApplyMenuLayout();
            }

            ImGui::EndTabItem();
        }

        // --------------------------------------------------------
        // TAB 2: DATA INSPECTOR (FITUR TAMBAHAN)
        // --------------------------------------------------------
        // Tab ini berguna untuk ngecek logic "Selected File" jalan atau tidak
        if (ImGui::BeginTabItem("Data Debugger"))
        {
            ImGui::Spacing();
            ImGui::Text("Current State Info:");
            ImGui::Separator();

            // Tampilkan apa yang tersimpan di variabel selectedFileName
            if (selectedFileName.empty())
            {
                ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Selected: (NONE)");
            }
            else
            {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Selected Key: %s", selectedFileName.c_str());

                // Cek apakah data benar-benar ada di map
                if (fileDatabase.count(selectedFileName))
                {
                    const auto& data = fileDatabase[selectedFileName];
                    ImGui::Text("Title: %s", data.title.c_str());
                    ImGui::Text("Line Count: %d", (int)data.lines.size());

                    // Preview isi teks baris pertama
                    if (!data.lines.empty()) {
                        ImGui::TextDisabled("Preview line 1: %s", data.lines[0].c_str());
                    }
                }
                else
                {
                    ImGui::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, "WARNING: Key exists but Data Missing in Map!");
                }
            }

            ImGui::EndTabItem();
        }

        // --------------------------------------------------------
        // TAB 3: POST PROCESS (Tetap)
        // --------------------------------------------------------
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

