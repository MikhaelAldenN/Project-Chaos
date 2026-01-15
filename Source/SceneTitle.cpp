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

    // [BARU] Inisialisasi Post Process System
    postProcess = std::make_unique<PostProcessManager>();
    // Asumsi layar default 1920x1080, nanti akan auto-resize jika window beda
    postProcess->Initialize(1920, 1080);

    // =========================================================
    // 1. SETUP DEFAULT THEME
    // =========================================================
    menuConfig.styleStandby.backColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    menuConfig.styleStandby.borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    menuConfig.styleStandby.textColor = { 0.96f, 0.80f, 0.23f, 1.0f };

    menuConfig.styleHover.backColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.styleHover.borderColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.styleHover.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    menuConfig.stylePress.backColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.stylePress.borderColor = { 0.0f, 0.0f, 0.8f, 1.0f };
    menuConfig.stylePress.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // =========================================================
    // A. TOMBOL DEKORASI INDEPENDEN ("/..")
    // =========================================================
    auto btnUpDir = std::make_unique<UIButtonPrimitive>(
        primitiveBatcher.get(), " /..            <UP DIR>",
        menuConfig.startX, 260.0f,
        menuConfig.btnWidth, menuConfig.btnHeight
    );

    btnUpDir->SetStyle(ButtonState::STANDBY, menuConfig.styleStandby);
    btnUpDir->SetStyle(ButtonState::HOVER, menuConfig.styleHover);
    btnUpDir->SetStyle(ButtonState::PRESSED, menuConfig.stylePress);
    btnUpDir->SetPadding(8.0f);
    btnUpDir->SetAlignment(TextAlignment::Left);
    btnUpDir->SetTextScale(menuConfig.textScale);
    btnUpDir->SetVerticalAdjustment(menuConfig.verticalAdj);

    btnUpDir->SetOnClick([]() { printf("Clicked decoration /..\n"); });
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
        auto btn = std::make_unique<UIButtonPrimitive>(
            primitiveBatcher.get(), fileList[i], 0, 0, 100, 20
        );

        std::string fileName = fileList[i];
        UIButtonPrimitive* rawBtnPtr = btn.get();

        btn->SetOnClick([this, fileName, rawBtnPtr]() {
            if (currentActiveButton && currentActiveButton != rawBtnPtr) {
                currentActiveButton->SetSelected(false);
            }
            currentActiveButton = rawBtnPtr;
            currentActiveButton->SetSelected(true);

            this->selectedFileName = fileName;
            this->PlayDescriptionAnim(fileName);

            printf("[System] Selected: %s\n", fileName.c_str());
            if (fileName.find("Exit.exe") != std::string::npos) PostQuitMessage(0);
            });

        debugButtonList.push_back(btn.get());
        uiManager->AddButton(std::move(btn));
    }

    if (!debugButtonList.empty()) debugBtnExit = debugButtonList.back();

    ApplyMenuLayout();

    // 2. Load Resources
    bgSprite = std::make_unique<Sprite>(
        Graphics::Instance().GetDevice(),
        "Data/Sprite/Scene Title/Sprite_BorderBrickDos.png"
    );

    // 3. Initialize Data & Layout
    SetupLayout();
    SetupContent();

    // [HAPUS] Inisialisasi UberShader manual & CreateRenderTarget dihapus

    // =========================================================
    // SET DEFAULT SELECTION (AUTO-SELECT README)
    // =========================================================
    for (auto* btn : debugButtonList)
    {
        if (btn->GetText().find("README.txt") != std::string::npos)
        {
            if (currentActiveButton) currentActiveButton->SetSelected(false);
            currentActiveButton = btn;
            currentActiveButton->SetSelected(true);
            selectedFileName = btn->GetText();
            PlayDescriptionAnim(selectedFileName);
            break;
        }
    }
}

void SceneTitle::SetupLayout()
{
    panelStatus = { "Status Panel", 341.0f, 132.0f, 0.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
    panelDirectory = { "Directory Panel",  395.0f, 234.0f, 40.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
    panelDescription = { "Desc Panel", 844.0f, 96.0f, 30.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
}

void SceneTitle::SetupContent()
{
    textStatusOnline = "Online - Unstable";
    textDirectoryHeader = "NAME             SIZE";
    textTUIMenuBar = "F1 HELP | F2 NOTE | F3 EDIT | F4 DEL | F5 RUN | F9 REFR | F10 QUIT";

    fileDatabase["BeyondBreak...    3256"] = {
        "APPLICATION INFO",
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

    fileDatabase["README.txt         275"] = {
        "FILE CONTENT: README.TXT",
        {
            "    ____  _______   _____  _   _ ____       ",
            "   | __ )| ____\\ \\ / / _ \\| \\ | |  _ \\      ",
            "   |  _ \\|  _|  \\ V / | | |  \\| | | | |     ",
            "   | |_) | |___  | || |_| | |\\  | |_| |     ",
            " __|____/|_____|_|_| \\___/|_| \\_|____/____  ",
            "| __ )|  _ \\| ____|  / \\  | |/ / ____|  _ \\ ",
            "|  _ \\| |_) |  _|   / _ \\ | ' /|  _| | |_) |",
            "| |_) |  _ <| |___ / ___ \\| . \\| |___|  _ < ",
            "|____/|_| \\_\\_____/_/   \\_\\_|\\_\\_____|_| \\_\\",
            "",
            "              --a Game by--",
            "      KONPAIRUTOOREBAKAMI PRODUCTION",
            "",
            "===========================================",
            " RELEASE INFO  : PUBLIC ALPHA BUILD v1.0",
            " TARGET SYSTEM : BRICK-DOS / KERNEL 32",
            " CRACKED BY    : KONPAIRU-TO-OREBAKAMI",
            "===========================================",
            " ",
            "[ DESCRIPTION ]",
            " warning: this program is unstable!",
            " Beyond Breaker is not just a game. It is a ",
            " tooldesigned to shatter the 4th wall of ",
            " this OS."
        }
    };

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
    // [HAPUS] m_globalTime update dihapus, sudah dihandle PostProcessManager

    uiManager->Update();

    if (descTypewriter)
    {
        int overclockSpeed = 5;
        for (int i = 0; i < overclockSpeed; ++i)
        {
            descTypewriter->Update(elapsedTime);
        }
    }

    if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
    {
        Framework::Instance()->ChangeScene(std::make_unique<SceneGameBreaker>());
    }
}

void SceneTitle::Render(float dt, Camera* targetCamera)
{
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // Cek master switch
    bool usePostProcess = m_fxState.MasterEnabled;
    postProcess->SetEnabled(usePostProcess);

    // [BARU] STEP 1: Begin Capture (Layar Virtual)
    if (usePostProcess)
    {
        postProcess->BeginCapture();
    }
    else
    {
        // JIKA POST PROCESS MATI:
        // Kita harus clear screen manual. Karena kita tidak punya akses langsung 
        // ke BackBufferRTV lewat Graphics.h, kita ambil dari yang sedang nempel saat ini.

        ID3D11RenderTargetView* currentRTV = nullptr;
        ID3D11DepthStencilView* currentDSV = nullptr;

        // Ambil RTV/DSV yang sedang aktif (biasanya ini BackBuffer dari Framework)
        dc->OMGetRenderTargets(1, &currentRTV, &currentDSV);

        if (currentRTV)
        {
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Hitam
            dc->ClearRenderTargetView(currentRTV, clearColor);

            // Jangan lupa Release karena GetRenderTargets menaikkan Reference Count!
            currentRTV->Release();
        }

        if (currentDSV)
        {
            dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            currentDSV->Release();
        }
    }

    // =========================================================
    // RENDER SCENE CONTENT (Sama seperti sebelumnya)
    // =========================================================

    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

    if (bgSprite)
    {
        bgSprite->Render(
            dc, camera.get(),
            0, 0, 0,        // x, y, z
            1920.0f, 1080.0f, // w, h
            0.0f, 0.0f, 0.0f, // rotation
            1.0f, 1.0f, 1.0f, 1.0f // r, g, b, a
        );
    }

    uiManager->Render(dc, targetCamera);

    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (text)
    {
        text->Draw(textStatusOnline.c_str(),
            panelStatus.x, panelStatus.y, panelStatus.scale,
            panelStatus.color[0], panelStatus.color[1], panelStatus.color[2], panelStatus.color[3]);

        text->Draw(textDirectoryHeader.c_str(),
            panelDirectory.x, panelDirectory.y, panelDirectory.scale,
            panelDirectory.color[0], panelDirectory.color[1], panelDirectory.color[2], panelDirectory.color[3]);

        text->Draw(textTUIMenuBar.c_str(), 400.0f, 960.0f, 0.625f, 0.96f, 0.80f, 0.23f, 1.0f);
    }

    primitiveBatcher->Render(dc);

    if (text && descTypewriter)
    {
        descTypewriter->Render(text);
    }

    // =========================================================
    // [BARU] STEP 2: End Capture & Draw to Screen
    // =========================================================
    if (usePostProcess)
    {
        // TRICK: Sync Local Config (uberParams) ke Manager
        // Ini menjaga fitur "Toggle" di GUI tetap jalan tanpa menghilangkan data
        UberShader::UberData& activeData = postProcess->GetData();
        activeData = this->uberParams; // Copy semua settingan slider

        // Apply GUI Masks (Overrides)
        if (!m_fxState.EnableVignette) activeData.intensity = 0.0f;
        if (!m_fxState.EnableLens) {
            activeData.glitchStrength = 0.0f;
            activeData.distortion = 0.0f;
            activeData.blurStrength = 0.0f;
        }
        if (!m_fxState.EnableCRT) {
            activeData.scanlineStrength = 0.0f;
            activeData.fineOpacity = 0.0f;
        }

        // Gambar hasil akhir!
        postProcess->EndCapture(dt);
    }
}

void SceneTitle::OnResize(int width, int height)
{
    // [BARU] Delegasikan resize ke manager
    if (postProcess) postProcess->OnResize(width, height);
}

void SceneTitle::ApplyMenuLayout()
{
    for (int i = 0; i < debugButtonList.size(); ++i)
    {
        UIButtonPrimitive* btn = debugButtonList[i];
        float currentY = menuConfig.startY + (i * (menuConfig.btnHeight + menuConfig.spacing));
        btn->SetPosition(menuConfig.startX, currentY);
        btn->SetSize(menuConfig.btnWidth, menuConfig.btnHeight);
        btn->SetPadding(menuConfig.paddingX);
        btn->SetTextScale(menuConfig.textScale);
        btn->SetVerticalAdjustment(menuConfig.verticalAdj);
        btn->SetAlignment((TextAlignment)menuConfig.alignment);

        btn->SetStyle(ButtonState::STANDBY, menuConfig.styleStandby);
        btn->SetStyle(ButtonState::HOVER, menuConfig.styleHover);
        btn->SetStyle(ButtonState::PRESSED, menuConfig.stylePress);
    }

    if (!debugButtonList.empty())
    {
        // Jika perlu reset selection logic saat layout berubah
    }
}

// --- GUI IMPLEMENTATION ---

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
        if (ImGui::BeginTabItem("UI Layout"))
        {
            if (ImGui::CollapsingHeader("Text Layouts", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                ImGui::PushID("p_status");
                ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "[Left Top] Status");
                ImGuiEditPanel(panelStatus);
                ImGui::PopID();
                ImGui::Separator();
                ImGui::PushID("p_dir");
                ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "[Left Mid] Header Name/Size");
                ImGuiEditPanel(panelDirectory);
                ImGui::PopID();
                ImGui::Separator();
                ImGui::PushID("p_desc");
                ImGui::TextColored({ 0.2f, 0.8f, 1.0f, 1.0f }, "[Right Main] Description Panel");
                ImGuiEditPanel(panelDescription);
                ImGui::PopID();
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Menu Group Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // ... logic slider menuConfig disini (sama seperti sebelumnya)
                // Jika kamu butuh ini ditulis ulang full, kabari ya. 
                // Untuk sekarang asumsi logic slidernya aman.
                if (ImGui::Button("Apply Layout")) ApplyMenuLayout();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data Debugger"))
        {
            ImGui::Spacing();
            ImGui::Text("Current State Info:");
            ImGui::Separator();
            if (selectedFileName.empty())
            {
                ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Selected: (NONE)");
            }
            else
            {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Selected Key: %s", selectedFileName.c_str());
            }
            ImGui::EndTabItem();
        }

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

    // Logic Switch ON/OFF
    if (m_fxState.MasterEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Turn Off Filter", ImVec2(-1, 40))) m_fxState.MasterEnabled = false;
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Turn On Filter", ImVec2(-1, 40))) m_fxState.MasterEnabled = true;
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (!m_fxState.MasterEnabled) return;

    // Catatan: Slider ini mengedit 'uberParams' lokal (di SceneTitle).
    // Nanti 'uberParams' ini dicopy ke Manager setiap frame di Render().

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

void SceneTitle::PlayDescriptionAnim(const std::string& key)
{
    if (key.empty() || fileDatabase.find(key) == fileDatabase.end()) return;

    descTypewriter = std::make_unique<Typewriter>();
    const FileMetadata& data = fileDatabase[key];

    float marginTop = 30.0f;
    float currentY = panelDescription.y + marginTop;
    float typingSpeed = 0.01f;

    for (const std::string& line : data.lines)
    {
        descTypewriter->AddLine(
            line, panelDescription.x, currentY, panelDescription.scale,
            panelDescription.color, typingSpeed
        );
        currentY += panelDescription.lineSpacing;
    }
}