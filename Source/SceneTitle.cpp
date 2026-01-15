#include "SceneTitle.h"
#include "SceneGameBreaker.h"
#include "Framework.h"
#include "ResourceManager.h"
#include "System/Input.h"
#include "System/Graphics.h"
#include "TUIBuilder.h"
#include <imgui.h>

SceneTitle::SceneTitle()
{
    // 1. Initialize Core Systems
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);
    camera->SetPosition(0.0f, 0.0f, -10.0f);

    TextDatabase::Instance().Initialize();

    primitiveBatcher = std::make_unique<Primitive>(Graphics::Instance().GetDevice());
    uiManager = std::make_unique<ButtonManager>(); 

    logConsole = std::make_unique<LogConsole>();
    logConsole->Initialize(335.0f, 695.0f, 6);
    logConsole->SetStyle(0.625f, 30.0f, 0.96f, 0.80f, 0.23f, 0.6f);
    logConsole->SetSpeed(0.5f, 5.0f);

    postProcess = std::make_unique<PostProcessManager>();
    postProcess->Initialize(1920, 1080);

    // 2. Configure Default Menu Theme
    menuConfig.styleStandby = { {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.96f, 0.80f, 0.23f, 1.0f} };
    menuConfig.styleHover = { {0.0f, 0.0f, 0.8f, 1.0f}, {0.0f, 0.0f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} };
    menuConfig.stylePress = { {0.0f, 0.0f, 0.8f, 1.0f}, {0.0f, 0.0f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} };

    // 3. Construct UI using TUIBuilder
    //TUIBuilder builder(uiManager.get(), primitiveBatcher.get());

    //TUITheme theme;
    //theme.styleStandby = menuConfig.styleStandby;
    //theme.styleHover = menuConfig.styleHover;
    //theme.stylePress = menuConfig.stylePress;
    //theme.textScale = menuConfig.textScale;
    //theme.padding = menuConfig.paddingX;
    //theme.verticalAdj = menuConfig.verticalAdj;
    //theme.align = (TextAlignment)menuConfig.alignment;

    //builder.SetTheme(theme);
    //builder.SetButtonSize(menuConfig.btnWidth, menuConfig.btnHeight);

    //// 3.1 Build Decoration
    //builder.SetStartPosition(menuConfig.startX, 260.0f);
    //builder.AddDecoration(" /..            <UP DIR>");

    //// 3.2 Build Main Menu
    //builder.SetStartPosition(menuConfig.startX, menuConfig.startY);
    //builder.SetSpacing(menuConfig.spacing);

    //const auto& fileList = TextDatabase::Instance().GetDirectoryList();
    //menuButtons.clear();

    //for (const auto& fileName : fileList)
    //{
    //    // Create button with deferred logic
    //    UIButtonPrimitive* btn = builder.AddButton(fileName, nullptr);

    //    btn->SetOnClick([this, fileName, btn]() {
    //        // Radio Button Logic
    //        if (currentActiveButton && currentActiveButton != btn) {
    //            currentActiveButton->SetSelected(false);
    //        }
    //        currentActiveButton = btn;
    //        currentActiveButton->SetSelected(true);

    //        // App Logic
    //        this->selectedFileName = fileName;
    //        this->PlayDescriptionAnim(fileName);
    //        if (this->logConsole)
    //        {
    //            std::string shortName = fileName.substr(0, fileName.find(" "));
    //            this->logConsole->AddLog("> Open: " + shortName, 0.85f);
    //        }
    //        printf("[System] Selected: %s\n", fileName.c_str());

    //        if (fileName.find("Exit.exe") != std::string::npos) PostQuitMessage(0);
    //        });

    //    menuButtons.push_back(btn);
    //}

    //if (!menuButtons.empty()) btnExit = menuButtons.back();

    // 4. Load Assets & Finalize Setup
    bgSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Title/Sprite_BorderBrickDos.png");

    SetupLayout();
    SetupContent();

    BuildMenu("ROOT");
}

void SceneTitle::BuildMenu(const std::string& folderName)
{
    // 1. Bersihkan Tombol Lama
    menuButtons.clear();
    uiManager->Clear();

    currentFolder = folderName;

    // 2. Siapkan Builder Ulang
    TUIBuilder builder(uiManager.get(), primitiveBatcher.get());

    // Setup Theme (Copy dari config)
    TUITheme theme;
    theme.styleStandby = menuConfig.styleStandby;
    theme.styleHover = menuConfig.styleHover;
    theme.stylePress = menuConfig.stylePress;
    theme.textScale = menuConfig.textScale;
    theme.padding = menuConfig.paddingX;
    theme.verticalAdj = menuConfig.verticalAdj;
    theme.align = (TextAlignment)menuConfig.alignment;

    builder.SetTheme(theme);
    builder.SetButtonSize(menuConfig.btnWidth, menuConfig.btnHeight);

    // =========================================================
    // [MODIFIKASI START]
    // =========================================================

    // A. Logic Dekorasi (Header)
    // HANYA TAMPILKAN DEKORASI JIKA DI ROOT.
    if (folderName == "ROOT")
    {
        builder.SetStartPosition(menuConfig.startX, 260.0f);
        builder.AddDecoration(" VOL (C:)          <SYS>");
    }
    // "Else" dihapus supaya folder Misc tidak punya header "SUB DIR"

    // B. Logic Posisi Tombol
    // Default Y = 299.0f (Sesuai config awal)
    float currentListY = menuConfig.startY;

    // Jika BUKAN ROOT (misal di Misc), kita "naikkan" posisi tombolnya 
    // ke 260.0f (posisi bekas header tadi) supaya tidak ompong.
    if (folderName != "ROOT")
    {
        currentListY = 260.0f;
    }

    builder.SetStartPosition(menuConfig.startX, currentListY);
    builder.SetSpacing(menuConfig.spacing);

    // =========================================================
    // [MODIFIKASI END]
    // =========================================================

    // 3. Ambil File dari Database berdasarkan Folder
    const auto& fileList = TextDatabase::Instance().GetFiles(folderName);

    for (const auto& fileName : fileList)
    {
        UIButtonPrimitive* btn = builder.AddButton(fileName, nullptr);

        btn->SetOnClick([this, fileName, btn]() {

            // --- [LOGIC NAVIGASI] ---

            // A. Tombol BACK ("../")
            if (fileName.find("../") != std::string::npos)
            {
                this->PlayDescriptionAnim("");
                this->pendingFolder = "ROOT";
                if (this->logConsole) this->logConsole->AddLog("> CD C:", 1.0f);
                return;
            }

            // B. Masuk Folder MISC ("Misc/")
            if (fileName.find("Misc/") != std::string::npos)
            {
                this->PlayDescriptionAnim(fileName);
                this->pendingFolder = "Misc";
                if (this->logConsole) this->logConsole->AddLog("> CD Misc", 1.0f);
                return;
            }

            // --- [LOGIC SELECT BIASA] ---
            if (currentActiveButton && currentActiveButton != btn) {
                currentActiveButton->SetSelected(false);
            }
            currentActiveButton = btn;
            currentActiveButton->SetSelected(true);

            this->selectedFileName = fileName;
            this->PlayDescriptionAnim(fileName);

            if (this->logConsole) {
                std::string shortName = fileName.substr(0, fileName.find(" "));
                this->logConsole->AddLog("> Open: " + shortName, 1.0f);
            }

            if (fileName.find("Exit.exe") != std::string::npos) PostQuitMessage(0);
            });

        menuButtons.push_back(btn);
    }

    if (!menuButtons.empty()) btnExit = menuButtons.back();

    if (folderName == "ROOT")
    {
        for (auto* btn : menuButtons)
        {
            // Cari tombol yang teksnya mengandung "README.txt"
            if (btn->GetText().find("README.txt") != std::string::npos)
            {
                // 1. Reset seleksi lama (jika ada)
                if (currentActiveButton) currentActiveButton->SetSelected(false);

                // 2. Set tombol README jadi terpilih
                currentActiveButton = btn;
                currentActiveButton->SetSelected(true);

                // 3. Load Konten ke Panel Kanan
                this->selectedFileName = btn->GetText();
                this->PlayDescriptionAnim(selectedFileName);

                // 4. Catat di Log (Opsional)
                //if (this->logConsole) this->logConsole->AddLog("> Default: README", 1.0f);

                break; // Berhenti mencari karena sudah ketemu
            }
        }
    }
}

// =========================================================
// CORE LOOP
// =========================================================

void SceneTitle::Update(float elapsedTime)
{
    if (!pendingFolder.empty())
    {
        BuildMenu(pendingFolder); // Rebuild menu di sini AMAN
        pendingFolder = "";       // Reset request
    }

    uiManager->Update();
    logConsole->Update(elapsedTime);

    if (descTypewriter)
    {
        // Overclock typewriter effect (5x speed)
        for (int i = 0; i < 5; ++i) descTypewriter->Update(elapsedTime);
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

    // --- STEP 1: Post-Process Capture ---
    postProcess->SetEnabled(m_fxState.MasterEnabled);

    if (m_fxState.MasterEnabled)
    {
        postProcess->BeginCapture();
    }
    else
    {
        // Fallback: Manually clear the active BackBuffer
        ID3D11RenderTargetView* currentRTV = nullptr;
        ID3D11DepthStencilView* currentDSV = nullptr;
        dc->OMGetRenderTargets(1, &currentRTV, &currentDSV);

        if (currentRTV) {
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            dc->ClearRenderTargetView(currentRTV, clearColor);
            currentRTV->Release();
        }
        if (currentDSV) {
            dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            currentDSV->Release();
        }
    }

    // --- STEP 2: Render Scene Objects ---
    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

    if (bgSprite)
    {
        bgSprite->Render(dc, camera.get(), 0, 0, 0, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    uiManager->Render(dc, targetCamera);
    primitiveBatcher->Render(dc);

    // Text Rendering
    BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
    if (font)
    {
        auto DrawPanelText = [&](const std::string& txt, const PanelLayout& p) {
            font->Draw(txt.c_str(), p.x, p.y, p.scale, p.color[0], p.color[1], p.color[2], p.color[3]);
            };

        DrawPanelText(textStatusOnline, panelStatus);
        DrawPanelText(textDirectoryHeader, panelDirectory);
        font->Draw(textTUIMenuBar.c_str(), 400.0f, 960.0f, 0.625f, 0.96f, 0.80f, 0.23f, 1.0f);

        if (descTypewriter) descTypewriter->Render(font);
        logConsole->Render(font);
    }

    // --- STEP 3: Apply Post-Processing & Present ---
    if (m_fxState.MasterEnabled)
    {
        // Sync GUI sliders to Shader Data
        UberShader::UberData& activeData = postProcess->GetData();
        activeData = this->uberParams;

        // Apply Logic Masks
        if (!m_fxState.EnableVignette) activeData.intensity = 0.0f;
        if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; activeData.blurStrength = 0.0f; }
        if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }

        postProcess->EndCapture(dt);
    }
}

void SceneTitle::OnResize(int width, int height)
{
    if (postProcess) postProcess->OnResize(width, height);
}

// =========================================================
// INTERNAL LOGIC
// =========================================================

void SceneTitle::SetupLayout()
{
    panelStatus = { "Status Panel", 341.0f, 132.0f, 0.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
    panelDirectory = { "Directory Panel", 395.0f, 234.0f, 40.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
    panelDescription = { "Desc Panel", 844.0f, 96.0f, 30.0f, 0.625f, {0.96f, 0.80f, 0.23f, 1.0f} };
}

void SceneTitle::SetupContent()
{
    textStatusOnline = TextDatabase::Instance().GetSystemString("StatusOnline");
    textDirectoryHeader = TextDatabase::Instance().GetSystemString("DirectoryHeader");
    textTUIMenuBar = TextDatabase::Instance().GetSystemString("TUIMenuBar");
}

void SceneTitle::ApplyMenuLayout()
{
    for (int i = 0; i < menuButtons.size(); ++i)
    {
        UIButtonPrimitive* btn = menuButtons[i];
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
}

void SceneTitle::PlayDescriptionAnim(const std::string& key)
{
    if (key.empty()) return;

    // [BARU] Bersihkan Key (Ambil nama file saja, buang spasi/size)
    // Contoh: "joke.txt       5" -> "joke.txt"
    std::string cleanKey = key;
    size_t spacePos = key.find(" ");
    if (spacePos != std::string::npos)
    {
        cleanKey = key.substr(0, spacePos);
    }

    // [UBAH] Gunakan cleanKey untuk mencari data
    const FileMetadata* data = TextDatabase::Instance().GetMetadata(cleanKey);

    if (!data) {
        // Debugging: Kalau masih ga muncul, cek output window
        printf("[Warning] Metadata not found for key: '%s'\n", cleanKey.c_str());
        return;
    }

    descTypewriter = std::make_unique<Typewriter>();

    float currentY = panelDescription.y + 30.0f;

    for (const std::string& line : data->lines)
    {
        descTypewriter->AddLine(line, panelDescription.x, currentY, panelDescription.scale, panelDescription.color, 0.01f);
        currentY += panelDescription.lineSpacing;
    }
}

// =========================================================
// DEBUG GUI IMPLEMENTATION
// =========================================================

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
                ImGuiEditPanel(panelStatus);
                ImGuiEditPanel(panelDirectory);
                ImGuiEditPanel(panelDescription);
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Menu Group Settings"))
            {
                // Note: Sliders for menuConfig should go here.
                if (ImGui::Button("Apply Layout")) ApplyMenuLayout();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data Debugger"))
        {
            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Selected: %s", selectedFileName.empty() ? "NONE" : selectedFileName.c_str());
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

void SceneTitle::ImGuiEditPanel(PanelLayout& layout)
{
    if (ImGui::TreeNode(layout.name))
    {
        ImGui::DragFloat("Pos X", &layout.x, 1.0f, 0.0f, 1920.0f);
        ImGui::DragFloat("Pos Y", &layout.y, 1.0f, 0.0f, 1080.0f);
        ImGui::DragFloat("Scale", &layout.scale, 0.01f, 0.1f, 5.0f);
        ImGui::ColorEdit4("Color", layout.color);
        ImGui::TreePop();
    }
}

void SceneTitle::GUIPostProcessTab()
{
    ImGui::Spacing();

    // Master Switch
    const char* label = m_fxState.MasterEnabled ? "Turn Off Filter" : "Turn On Filter";
    ImVec4 color = m_fxState.MasterEnabled ? ImVec4(0.6f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 0.2f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    if (ImGui::Button(label, ImVec2(-1, 40))) m_fxState.MasterEnabled = !m_fxState.MasterEnabled;
    ImGui::PopStyleColor();

    if (!m_fxState.MasterEnabled) return;

    ImGui::Separator();

    auto CheckboxLayer = [&](const char* label, bool& val) {
        ImGui::Checkbox(label, &val);
        if (!val) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        };

    // Vignette
    if (ImGui::CollapsingHeader("Vignette & Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        CheckboxLayer("ACTIVATE: Vignette", m_fxState.EnableVignette);
        ImGui::ColorEdit3("Tint", &uberParams.color.x);
        ImGui::SliderFloat("Intensity", &uberParams.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &uberParams.smoothness, 0.01f, 1.0f);
        ImGui::Checkbox("Rounded", &uberParams.rounded);
        if (!m_fxState.EnableVignette) ImGui::PopStyleVar();
    }

    // Lens
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        CheckboxLayer("ACTIVATE: Lens", m_fxState.EnableLens);
        ImGui::SliderFloat("Fisheye", &uberParams.distortion, -0.5f, 0.5f);
        ImGui::SliderFloat("Chroma", &uberParams.blurStrength, 0.0f, 0.05f);
        ImGui::SliderFloat("Glitch", &uberParams.glitchStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableLens) ImGui::PopStyleVar();
    }

    // CRT
    if (ImGui::CollapsingHeader("CRT Monitor"))
    {
        CheckboxLayer("ACTIVATE: CRT", m_fxState.EnableCRT);
        ImGui::SliderFloat("Density", &uberParams.fineDensity, 10.0f, 500.0f);
        ImGui::SliderFloat("Opacity", &uberParams.fineOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Speed", &uberParams.scanlineSpeed, -10.0f, 10.0f);
        ImGui::SliderFloat("Scan Opacity", &uberParams.scanlineStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableCRT) ImGui::PopStyleVar();
    }
}