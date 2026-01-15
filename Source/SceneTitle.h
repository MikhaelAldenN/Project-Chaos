#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Scene.h"
#include "Camera.h"
#include "Primitive.h"
#include "ButtonManager.h"
#include "UIButtonPrimitive.h"
#include "Typewriter.h"
#include "System/Sprite.h" 
#include "TextDatabase.h"
#include "PostProcessManager.h"
#include "LogConsole.h"

// Structs for UI Layout
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

    // Core Loop
    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void OnResize(int width, int height) override;

    // Debug / Tools
    void DrawGUI() override;
    Camera* GetCamera() const { return camera.get(); }

private:
    // --- Subsystems ---
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Sprite> bgSprite;
    std::unique_ptr<Primitive> primitiveBatcher;
    std::unique_ptr<ButtonManager> uiManager;
    std::unique_ptr<PostProcessManager> postProcess;
    std::unique_ptr<Typewriter> descTypewriter;
    std::unique_ptr<LogConsole> logConsole;

    // --- Layout & Visual Configuration ---
    PanelLayout panelStatus;
    PanelLayout panelDirectory;
    PanelLayout panelDescription;

    struct MenuConfig {
        float startX = 340.0f;
        float startY = 299.0f;
        float btnWidth = 421.0f;
        float btnHeight = 40.0f;
        float spacing = 0.0f;
        float paddingX = 10.0f;
        float textScale = 0.625f;
        float verticalAdj = 2.0f;
        int alignment = 2; // Left

        ButtonStyle styleStandby;
        ButtonStyle styleHover;
        ButtonStyle stylePress;
    } menuConfig;

    // --- State Management ---
    std::string selectedFileName = "";
    UIButtonPrimitive* currentActiveButton = nullptr;
    UIButtonPrimitive* btnExit = nullptr;
    std::vector<UIButtonPrimitive*> menuButtons;

    // --- Cached Strings ---
    std::string textStatusOnline;
    std::string textDirectoryHeader;
    std::string textTUIMenuBar;

    // --- Post Process State ---
    struct PostProcessState {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    } m_fxState;

    UberShader::UberData uberParams; // Local copy for GUI manipulation

    // --- Internal Helpers ---
    void SetupLayout();
    void SetupContent();
    void ApplyMenuLayout();
    void PlayDescriptionAnim(const std::string& key);

    // --- Debug GUI Helpers ---
    void GUIPostProcessTab();
    void ImGuiEditPanel(PanelLayout& layout);
};