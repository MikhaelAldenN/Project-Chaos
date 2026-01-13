#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include "Ball.h"
#include "BlockManager.h"
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

    // Return raw pointer untuk kompatibilitas, tapi ownership tetap di shared_ptr
    Camera* GetMainCamera() const { return mainCamera.get(); }

private:
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);
    void UpdateAnimation(float elapsedTime);
    void CreateRenderTarget();

    // =========================================================
    // ASSETS & OBJECTS
    // =========================================================
    Ball* ball = nullptr;
    Paddle* paddle = nullptr;
    Player* player = nullptr;
    std::unique_ptr<BlockManager> blockManager;

    // =========================================================
    // CAMERA SYSTEM (REFACTORED)
    // =========================================================
    // [UPDATE] Menggunakan Smart Pointer (Ownership jelas)
    std::shared_ptr<Camera> mainCamera;

    // [UPDATE] Menggunakan Struct Data (Hemat Memori), bukan Objek Camera dummy
    // Kita inisialisasi dengan data default (0,0,0)
    CameraKeyframe m_poseA{ {0,0,0}, {0,0,0} };
    CameraKeyframe m_poseB{ {0,0,0}, {0,0,0} };
    CameraKeyframe m_poseC{ {0,0,0}, {0,0,0} };

    // State GUI
    float transitionDuration = 1.5f;
    int currentEasingIndex = 3; // SmoothStep

    // --- Graphics ---
    std::unique_ptr<Sprite> m_backgroundSprite;

    // =========================================================
    // SCENE SETTINGS 
    // =========================================================
    float initialFOV = 45.0f;
    float cameraNearZ = 0.1f;
    float cameraFarZ = 1000.0f;

    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    int triggerBlockCount = 40;
    float ballSpawnZOffset = 0.3f;

    // Animation settings
    float animDuration = 2.0f;
    float animTargetBgRotation = -180.0f;
    float animCameraRotationTotal = DirectX::XM_PI;

    // Render settings
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* backgroundPath = "Data/Sprite/Placeholder/[PLACEHOLDER]Back_Title.png";

    // Post Process
    std::unique_ptr<VignetteShader> vignetteShader;
    VignetteShader::VignetteData vignetteParams;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

    // Internal State
    bool m_isAnimating = false;
    bool m_isGlitching = false;
    bool m_hasTriggered = false;
    float m_animTimer = 0.0f;
    float m_globalTime = 0.0f;
    float m_bgRotation = 0.0f;
    DirectX::XMFLOAT2 m_bgPosition = { 0.0f, 0.0f };
};