#include "SceneTitle.h"
#include "SceneGameBreaker.h"
#include "Framework.h"
#include "System/Input.h"
#include "System/Graphics.h"
#include <imgui.h>

SceneTitle::SceneTitle()
{
    // 1. Setup Camera (Orthographic Fixed 1920x1080)
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->SetRotation(0.0f, 0.0f, 0.0f);

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

    ImGui::Separator();
    ImGui::Text("Press ENTER to Start Game");

    ImGui::End();
}