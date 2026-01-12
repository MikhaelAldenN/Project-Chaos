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
}

void SceneTitle::OnResize(int width, int height)
{
    // Keep empty for fixed aspect ratio games
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

            // 1. Edit Transform (Posisi & Ukuran)
            // Kita pakai static array sementara untuk menampung nilai dari Getter
            float pos[2] = { debugBtnExit->GetX(), debugBtnExit->GetY() };
            float size[2] = { debugBtnExit->GetWidth(), debugBtnExit->GetHeight() };

            // Jika slider digeser, kita panggil Setter
            if (ImGui::DragFloat2("Position", pos))
            {
                debugBtnExit->SetPosition(pos[0], pos[1]);
            }
            // Karena UIButton belum punya SetSize, kita skip resize atau kamu bisa tambah SetSize di UIButton.h

            if (ImGui::DragFloat2("Size", size, 1.0f, 10.0f, 1000.0f))
            {
                debugBtnExit->SetSize(size[0], size[1]);
            }

            if (ImGui::DragFloat2("Size", size, 1.0f, 10.0f, 1000.0f))
            {
                debugBtnExit->SetSize(size[0], size[1]);
            }

            // --- TAMBAHKAN INI ---
            // Edit Text Scale (Min 0.1, Max 5.0)
            ImGui::DragFloat("Text Scale", &debugBtnExit->GetTextScale(), 0.01f, 0.1f, 5.0f);

            // --- TAMBAHKAN INI (Alignment & Padding) ---
            ImGui::Separator();
            ImGui::Text("Text Layout");

            // 1. Padding Slider
            ImGui::DragFloat("Padding X", &debugBtnExit->GetPadding(), 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("V-Adjust (Y)", &debugBtnExit->GetVerticalAdjustment(), 0.5f, -50.0f, 50.0f);
            // 2. Alignment Dropdown
            // Kita butuh variabel int sementara untuk menampung state Combo box
            int alignState = (int)debugBtnExit->GetAlignment();
            const char* items[] = { "Center", "Left", "Right" };

            if (ImGui::Combo("Alignment", &alignState, items, IM_ARRAYSIZE(items)))
            {
                // Kembalikan int ke Enum saat user memilih
                debugBtnExit->SetAlignment((TextAlignment)alignState);
            }
            // -------------------------------------------

            ImGui::Separator();
            ImGui::Text("Color Pallete (RGB)");

            // 2. Edit Warna (Langsung tembak ke reference memory via Getter tadi)
            // Kita perlu casting dari XMFLOAT3 ke float* array agar dimengerti ImGui
            ImGui::ColorEdit3("Normal", &debugBtnExit->GetColorNormal().x);
            ImGui::ColorEdit3("Hover", &debugBtnExit->GetColorHover().x);
            ImGui::ColorEdit3("Pressed", &debugBtnExit->GetColorPress().x);
            ImGui::ColorEdit3("Border", &debugBtnExit->GetColorBorder().x);
        }
    }

    ImGui::Separator();
    ImGui::Text("Press ENTER to Start Game");

    ImGui::End();
}