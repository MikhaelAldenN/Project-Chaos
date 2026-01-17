#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h> 

// Game Systems
#include "Scene.h"
#include "GameWindow.h"
#include "CameraController.h"
#include "UberShader.h"
#include "System/Sprite.h"
#include "BitmapFont.h"     
#include "ResourceManager.h"

// Game Objects
#include "Ball.h"
#include "Paddle.h"
#include "Player.h"
#include "BlockManager.h"

struct SceneCameraPoint {
    std::string Name;

    // START STATE
    DirectX::XMFLOAT3 StartPos;
    DirectX::XMFLOAT3 StartLookAt;

    // END STATE
    DirectX::XMFLOAT3 EndPos;
    DirectX::XMFLOAT3 EndLookAt;

    float Duration;
    EasingType Easing;
};

class SceneGameBreaker : public Scene
{
public:
    SceneGameBreaker();
    ~SceneGameBreaker() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return mainCamera.get(); }

private:
    // =========================================================
    // DEFINE STRUCT 
    // =========================================================
    struct UI_LayoutData {
        char name[32];
        float x, y;
        float lineSpacing;
        float scale;
        float color[4];
    };

    // =========================================================
    // INTERNAL HELPER FUNCTIONS
    // =========================================================
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);
    void CreateRenderTarget();
    void PlayCinematicTrigger();

    // --- GUI Helpers ---
    void GUICameraTab();       // Tab Inspector: Kamera (Sekarang hanya status)
    void GUIPostProcessTab();  // Tab Inspector: Efek Visual & Filter
    void GUISpriteTab();       // Tab Inspector: Sprite Border Breaker
    void GUISectionHeader(const char* label);
    void ImGuiEditPanel(UI_LayoutData& layout);

    // =========================================================
    // SPRITE CONTROLS
    // =========================================================
    DirectX::XMFLOAT3 m_spritePos = { 0.143f, 13.0f, 0.0f };
    DirectX::XMFLOAT2 m_spriteSize = { 16.0f, 9.0f };
    float m_spriteScale = 0.645f;
    float m_spritePitch = 90.0f;
    float m_spriteYaw = 0.0f;
    float m_spriteRoll = 0.0f;

    // =========================================================
    // LAYOUT DATA
    // =========================================================
    std::string tutorialText = "[A/D] Move [SPACE] Shoot";

    UI_LayoutData tutorialLayout = {
         "Tutorial Panel",               // name
         349.0f, 836.0f,                 // x, y
         0.0f,                           // lineSpacing
         0.625f,                         // scale
         { 0.96f, 0.80f, 0.23f, 1.0f }   // color 
    };

    // =========================================================
    // INTERNAL DATA STRUCTURES
    // =========================================================
    struct PostProcessState
    {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };

    // =========================================================
    // 1. GAMEPLAY OBJECTS
    // =========================================================
    Ball* ball = nullptr;
    Paddle* paddle = nullptr;
    Player* player = nullptr;
    std::unique_ptr<BlockManager> blockManager;

    // =========================================================
    // 2. CAMERA SYSTEM
    // =========================================================
    std::shared_ptr<Camera> mainCamera;
    std::vector<SceneCameraPoint> m_camScenarioPoints; // A, B, C

    // Helper untuk mengubah Pos + Target menjadi Rotasi (Pitch, Yaw, Roll)
    DirectX::XMFLOAT3 CalculateRotationFromTarget(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target);

    // Camera Settings
    float initialFOV = 45.0f;
    float cameraNearZ = 0.1f;
    float cameraFarZ = 1000.0f;
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // =========================================================
    // 3. GRAPHICS & POST-PROCESSING
    // =========================================================
    std::unique_ptr<Sprite> m_spriteBorderBreaker;
    std::unique_ptr<Sprite> m_spriteDEBUG_LAYOUT;

    std::unique_ptr<UberShader> uberShader; 
    UberShader::UberData        uberParams; 
    PostProcessState m_fxState;

    // DirectX Resources (Render Targets)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

    // =========================================================
    // 4. CONFIGURATION & SETTINGS
    // =========================================================
    // Gameplay Config
    int triggerBlockCount = 40;
    float ballSpawnZOffset = 0.35f;

    // Visual Config
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* pathBorderBreaker = "Data/Sprite/Scene Breaker/Sprite_BorderBreakertransparent.png";
	//const char* pathDebugLayout = "Data/Sprite/Placeholder/[PLACEHOLDER]Back_Title.png";

    // =========================================================
    // 5. RUNTIME STATE (VARIABLES)
    // =========================================================
    bool m_hasTriggered = false; // Flag untuk trigger logic game (bukan animasi)
    float m_globalTime = 0.0f;

    // Background dynamic state
    float m_bgRotation = 0.0f;

    bool m_isShakeEnabled = false;
};