#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include "Ball.h"
#include "BlockManager.h"
#include "Camera.h"
#include "CameraController.h"
#include "GameWindow.h"
#include "Paddle.h"
#include "Player.h"
#include "Scene.h"
#include "VignetteShader.h"
#include "System/Sprite.h"

class SceneGameBreaker : public Scene
{
public:
    SceneGameBreaker();
    ~SceneGameBreaker() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return mainCamera; }
    Camera* GetSubCamera() const { return subCamera; }

private:
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);
    void UpdateAnimation(float elapsedTime);
    void CreateRenderTarget();

    // =========================================================
    // ASSETS & OBJECTS
    // =========================================================
    // --- Main Game Objects ---
    Ball* ball = nullptr;
    Paddle* paddle = nullptr;
    Player* player = nullptr;
    std::unique_ptr<BlockManager> blockManager;

    // --- Cameras ---
    Camera* mainCamera = nullptr;
    Camera* subCamera = nullptr;
    std::vector<Camera*> additionalCameras;

    // --- Windows ---
    GameWindow* trackingWindow = nullptr;
    GameWindow* lensWindow = nullptr;
    Camera* trackingCamera = nullptr;
    Camera* lensCamera = nullptr;

    // --- Graphics ---
    std::unique_ptr<Sprite> m_backgroundSprite;

    // =========================================================
    // SCENE SETTINGS 
    // =========================================================

    // ---------------------------------------------------------
    // CAMERA CONFIGURATION
    // ---------------------------------------------------------
    float initialFOV = 45.0f;                       // Field of View in Degrees
    float cameraNearZ = 0.1f;                       // Near Clipping Plane
    float cameraFarZ = 1000.0f;                     // Far Clipping Plane

    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };   // Top-down view
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // ---------------------------------------------------------
    // GAMEPLAY SETTINGS
    // ---------------------------------------------------------
    int triggerBlockCount = 40;                     // Remaining blocks to trigger Breakout
    float ballSpawnZOffset = 0.3f;                  // How far in front of paddle the ball sits before launch

    // ---------------------------------------------------------
    // ANIMATION SETTINGS
    // ---------------------------------------------------------
    float animDuration = 2.0f;                      // How long the spin takes
    float animTargetBgRotation = -180.0f;           // Total background rotation (degrees)
    float animCameraRotationTotal = DirectX::XM_PI; // Total camera spin (radians)

    // ---------------------------------------------------------
    // RENDER SETTINGS
    // ---------------------------------------------------------
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Tint (RGBA)
    const char* backgroundPath = "Data/Sprite/Placeholder/[PLACEHOLDER]Back_Title.png";

    // =========================================================
    // POST PROCESS RESOURCES
    // =========================================================
    std::unique_ptr<VignetteShader> vignetteShader;
    VignetteShader::VignetteData vignetteParams;

    // Render Target Resources (To draw the scene into a texture)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture; 
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

    // ---------------------------------------------------------
    // INTERNAL STATE 
    // ---------------------------------------------------------
    bool m_isAnimating = false; 
    bool m_isGlitching = false;
    bool m_hasTriggered = false;        
    float m_animTimer = 0.0f;
    float m_globalTime = 0.0f;
    float m_bgRotation = 0.0f;
    DirectX::XMFLOAT2 m_bgPosition = { 0.0f, 0.0f };
};