#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h> // Untuk Microsoft::WRL::ComPtr

// Game Systems
#include "Scene.h"
#include "GameWindow.h"
#include "CameraController.h"
#include "VignetteShader.h"
#include "System/Sprite.h"

// Game Objects
#include "Ball.h"
#include "Paddle.h"
#include "Player.h"
#include "BlockManager.h"

class SceneGameBreaker : public Scene
{
public:
    SceneGameBreaker();
    ~SceneGameBreaker() override;

    // =========================================================
    // OVERRIDE METHODS (SCENE LIFECYCLE)
    // =========================================================
    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    // =========================================================
    // PUBLIC ACCESSORS
    // =========================================================
    // Mengembalikan raw pointer untuk kompatibilitas, ownership tetap di shared_ptr
    Camera* GetMainCamera() const { return mainCamera.get(); }

private:
    // =========================================================
    // INTERNAL HELPER FUNCTIONS
    // =========================================================
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);
    void UpdateAnimation(float elapsedTime);
    void CreateRenderTarget();

    // --- GUI Helpers ---
    void GUICameraTab();       // Tab Inspector: Kamera & Animasi
    void GUIPostProcessTab();  // Tab Inspector: Efek Visual & Filter
	void GUISpriteTab();       // Tab Inspector: Sprite Border Breaker
    void GUISectionHeader(const char* label);

    // =========================================================
    // SPRITE CONTROLS
    // =========================================================
    DirectX::XMFLOAT3 m_spritePos = { 0.110f, 15.0f, 0.0f }; // Y = -5 biar ada di bawah objek game
    DirectX::XMFLOAT2 m_spriteSize = { 16.0f, 9.0f };

    // [BARU] Ini untuk memperbesar/memperkecil total ukurannya
    float m_spriteScale = 0.460f;
    float m_spritePitch = 90.0f; // Rotasi X (90 derajat = Tidur)
    float m_spriteYaw = 0.0f;  // Rotasi Y
    float m_spriteRoll = 0.0f;  // Rotasi Z (Spinning)

    // =========================================================
    // INTERNAL DATA STRUCTURES
    // =========================================================
    struct PostProcessState
    {
        bool MasterEnabled = false; // Master Switch (On/Off seluruh efek)
        bool EnableVignette = true;  // Sub-switch: Vignette
        bool EnableLens = true;  // Sub-switch: Lens Distortion
        bool EnableCRT = true;  // Sub-switch: CRT/Scanline
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

    // Keyframes untuk Camera Sequence (Posisi A -> B -> C)
    CameraKeyframe m_poseA{ {0,0,0}, {0,0,0} };
    CameraKeyframe m_poseB{ {0,0,0}, {0,0,0} };
    CameraKeyframe m_poseC{ {0,0,0}, {0,0,0} };

    // Camera Settings
    float initialFOV = 45.0f;
    float cameraNearZ = 0.1f;
    float cameraFarZ = 1000.0f;
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // Camera Transition State (GUI)
    float transitionDuration = 1.5f;
    int currentEasingIndex = 3; // Default: SmoothStep

    // =========================================================
    // 3. GRAPHICS & POST-PROCESSING
    // =========================================================
    std::unique_ptr<Sprite> m_spriteBorderBreaker;
    std::unique_ptr<Sprite> m_spriteDEBUG_LAYOUT;

    // Shader System
    std::unique_ptr<VignetteShader> vignetteShader;
    VignetteShader::VignetteData vignetteParams;
    PostProcessState m_fxState; // State untuk GUI Checkbox

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
    float ballSpawnZOffset = 0.3f;

    // Visual Config
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* pathBorderBreaker = "Data/Sprite/Scene Breaker/Sprite_BorderBreakertransparent.png";
    const char* pathDEBUG_LAYOUT = "Data/Sprite/Placeholder/[PLACEHOLDER]Back_Title.png";

    // Animation Config
    float animDuration = 2.0f;
    float animTargetBgRotation = -180.0f;
    float animCameraRotationTotal = DirectX::XM_PI;

    // =========================================================
    // 5. RUNTIME STATE (VARIABLES)
    // =========================================================
    bool m_isAnimating = false;
    bool m_hasTriggered = false;
    bool m_isGlitching = false; // (Legacy/Opsional jika masih dipakai logic lama)

    float m_animTimer = 0.0f;
    float m_globalTime = 0.0f;

    // Background dynamic state
    float m_bgRotation = 0.0f;
    DirectX::XMFLOAT2 m_bgPosition = { 0.0f, 0.0f };
};