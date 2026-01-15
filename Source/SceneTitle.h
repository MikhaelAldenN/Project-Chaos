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
#include "UberShader.h"
#include "Typewriter.h"

struct FileMetadata {
    std::string title;            // Judul (misal: "App Details" atau "CREDITS")
    std::vector<std::string> lines; // Baris-baris teks deskripsi
};

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
    PanelLayout panelStatus; // Tetap (Online Status)

    // [BARU] Header untuk "Name Size"
    PanelLayout panelDirectory;

    PanelLayout panelDescription;

    // [HAPUS] dirPanel & logPanel sudah tidak dipakai

    std::map<std::string, FileMetadata> fileDatabase;
    std::string selectedFileName = "";

    // --- Content Data ---
    std::string textStatusOnline;
    std::string textDirectoryHeader;

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

    UIButtonPrimitive* debugBtnExit = nullptr; // Pointer ke tombol yang SEDANG diedit

    // Tambahkan 2 baris ini:
    std::vector<UIButtonPrimitive*> debugButtonList; // List semua tombol untuk dipilih
    int debugSelectedIdx = 0;                        // Index tombol yang dipilih sekarang

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

    // --- GROUP CONFIGURATION ---
    // Struct ini mengontrol seluruh layout menu secara global
    struct MenuConfig {
        // --- Layout (Existing) ---
        float startX = 340.0f;
        float startY = 299.0f;
        float btnWidth = 421.0f;
        float btnHeight = 40.0f;
        float spacing = 0.0f;
        float paddingX = 10.0f;
        float textScale = 0.625f;
        float verticalAdj = 2.0f;
        int alignment = 2;

        // --- Colors (NEW FEATURES) ---
        // Kita simpan 3 style utama di sini
        ButtonStyle styleStandby;
        ButtonStyle styleHover;
        ButtonStyle stylePress;
    };
    MenuConfig menuConfig; // Instance konfigurasi

    // Fungsi helper untuk menerapakan config ke semua tombol
    void ApplyMenuLayout();

    UIButtonPrimitive* currentActiveButton = nullptr;

    std::unique_ptr<Typewriter> descTypewriter;

    // Fungsi helper untuk mereset dan mengisi animasi teks
    void PlayDescriptionAnim(const std::string& key);
};