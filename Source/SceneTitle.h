#pragma once

#include <memory>
#include <string>
#include <vector>
#include <DirectXMath.h>

#include "Scene.h"
#include "System/Sprite.h" 
#include "Camera.h"
#include "imgui.h"

// Struct sederhana untuk data Layout
struct PanelLayout {
    char name[32];   // Nama untuk label ImGui
    float x, y;      // Posisi
    float lineSpacing;
    float scale;
    float color[4];  // Warna teks (R, G, B, A)
};

class SceneTitle : public Scene
{
public:
    SceneTitle();
    ~SceneTitle() override = default;

    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetCamera() const { return camera.get(); }

private:
    // --- Core Systems ---
    std::unique_ptr<Sprite> bgSprite;
    std::unique_ptr<Camera> camera;

    // --- Layout Configuration ---
    PanelLayout statusPanel;
    PanelLayout dirPanel;
    PanelLayout logPanel;

    // --- Content Data ---
    std::string statusText;
    std::vector<std::string> directoryFiles;
    std::vector<std::string> systemLogs;

    // --- Helper Functions ---
    void SetupLayout();
    void SetupContent();

    // Fungsi render teks generik
    void DrawListInPanel(const std::vector<std::string>& list, const PanelLayout& layout);

    // Fungsi helper ImGui untuk mengurangi duplikasi kode UI
    void ImGuiEditPanel(PanelLayout& layout);
};