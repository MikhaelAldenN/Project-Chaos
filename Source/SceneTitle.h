#pragma once

#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <map>

#include "Scene.h"
#include "System/Sprite.h" 
#include "Camera.h"
#include "imgui.h"
#include "Primitive.h"
#include "ButtonManager.h"
#include "UIButtonPrimitive.h"
#include "Typewriter.h"

// [BARU] Include Manager
#include "PostProcessManager.h"

struct FileMetadata {
    std::string title;
    std::vector<std::string> lines;
};

struct PanelLayout {
    char name[32];
    float x, y;
    float lineSpacing;
    float scale;
    float color[4];
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

    std::unique_ptr<Primitive> primitiveBatcher;
    std::unique_ptr<ButtonManager> uiManager;

    // [BARU] Post Process Manager
    // Menggantikan UberShader & RenderTarget manual
    std::unique_ptr<PostProcessManager> postProcess;

    // --- Layout Configuration ---
    PanelLayout panelStatus;
    PanelLayout panelDirectory;
    PanelLayout panelDescription;

    std::map<std::string, FileMetadata> fileDatabase;
    std::string selectedFileName = "";

    // --- Content Data ---
    std::string textStatusOnline;
    std::string textDirectoryHeader;
    std::string textTUIMenuBar;

    std::vector<std::string> directoryFiles;
    std::vector<std::string> systemLogs;

    // --- Helper Functions ---
    void SetupLayout();
    void SetupContent();
    // void CreateRenderTarget(); // [HAPUS] Sudah dihandle Manager
    void GUIPostProcessTab();

    void DrawListInPanel(const std::vector<std::string>& list, const PanelLayout& layout);
    void ImGuiEditPanel(PanelLayout& layout);

    UIButtonPrimitive* debugBtnExit = nullptr;
    std::vector<UIButtonPrimitive*> debugButtonList;
    int debugSelectedIdx = 0;

    // GUI State (Tetap disimpan untuk kontrol logic toggle)
    struct PostProcessState
    {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };
    PostProcessState m_fxState;

    // Kita simpan "Config" di sini. 
    // Saat Render, kita copy ini ke PostProcessManager.
    UberShader::UberData uberParams;

    // [HAPUS] m_globalTime dipindah ke Manager
    // [HAPUS] Semua ComPtr RenderTarget & DepthStencil

    // --- GROUP CONFIGURATION ---
    struct MenuConfig {
        float startX = 340.0f;
        float startY = 299.0f;
        float btnWidth = 421.0f;
        float btnHeight = 40.0f;
        float spacing = 0.0f;
        float paddingX = 10.0f;
        float textScale = 0.625f;
        float verticalAdj = 2.0f;
        int alignment = 2;

        ButtonStyle styleStandby;
        ButtonStyle styleHover;
        ButtonStyle stylePress;
    };
    MenuConfig menuConfig;

    void ApplyMenuLayout();

    UIButtonPrimitive* currentActiveButton = nullptr;

    std::unique_ptr<Typewriter> descTypewriter;
    void PlayDescriptionAnim(const std::string& key);
};