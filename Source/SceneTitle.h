#pragma once

#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Scene.h"
#include "System/Sprite.h" 
#include "Camera.h"
#include "imgui.h"
#include "Primitive.h"
#include "ButtonManager.h"
#include "UIButtonPrimitive.h"
#include "UberShader.h"

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

    std::unique_ptr<Primitive> primitiveBatcher;
    std::unique_ptr<ButtonManager> uiManager;

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
    void CreateRenderTarget(); 
    void GUIPostProcessTab();  

    // Fungsi render teks generik
    void DrawListInPanel(const std::vector<std::string>& list, const PanelLayout& layout);

    // Fungsi helper ImGui untuk mengurangi duplikasi kode UI
    void ImGuiEditPanel(PanelLayout& layout);

    UIButtonPrimitive* debugBtnExit = nullptr;

    // GUI State
    struct PostProcessState
    {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };
    PostProcessState m_fxState;

    // Post-Processing Resources
    std::unique_ptr<UberShader> uberShader;
    UberShader::UberData uberParams;
    float m_globalTime = 0.0f;

    // DirectX Render Targets
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
};