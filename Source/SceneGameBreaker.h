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

#include "CinematicDirector.h" // Logic Kamera ada di sini
#include "GUISceneGameBreaker.h" // Logic GUI ada di sini

class SceneGameBreaker : public Scene
{
    // Memberi akses ke class GUI untuk edit variabel private
    friend class GameBreakerGUI;

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
    // INTERNAL HELPER FUNCTIONS
    // =========================================================
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);
    void CreateRenderTarget();

    // =========================================================
    // SPRITE CONTROLS & LAYOUT
    // =========================================================
    struct UI_LayoutData {
        char name[32];
        float x, y;
        float lineSpacing;
        float scale;
        float color[4];
    };

    DirectX::XMFLOAT3 m_spritePos = { 0.143f, 13.0f, 0.0f };
    DirectX::XMFLOAT2 m_spriteSize = { 16.0f, 9.0f };
    float m_spriteScale = 0.645f;
    float m_spritePitch = 90.0f;
    float m_spriteYaw = 0.0f;
    float m_spriteRoll = 0.0f;

    std::string tutorialText = "[A/D] Move [SPACE] Shoot";
    UI_LayoutData tutorialLayout = {
         "Tutorial Panel", 349.0f, 836.0f, 0.0f, 0.625f, { 0.96f, 0.80f, 0.23f, 1.0f }
    };

    // =========================================================
    // RUNTIME STATE
    // =========================================================
    struct PostProcessState {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };

    Ball* ball = nullptr;
    Paddle* paddle = nullptr;
    Player* player = nullptr;
    std::unique_ptr<BlockManager> blockManager;

    std::shared_ptr<Camera> mainCamera;

    // Camera Settings
    float initialFOV = 45.0f;
    float cameraNearZ = 0.1f;
    float cameraFarZ = 1000.0f;
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // Graphics
    std::unique_ptr<Sprite> m_spriteBorderBreaker;
    std::unique_ptr<Sprite> m_spriteDEBUG_LAYOUT; // Jika masih dipakai

    std::unique_ptr<UberShader> uberShader;
    UberShader::UberData        uberParams;
    PostProcessState m_fxState;

    // Render Targets
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;

    // Config
    int triggerBlockCount = 40;
    float ballSpawnZOffset = 0.35f;
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* pathBorderBreaker = "Data/Sprite/Scene Breaker/Sprite_BorderBreakertransparent.png";

    // State Variables
    bool m_hasTriggered = false;
    float m_globalTime = 0.0f;
    float m_bgRotation = 0.0f;
    bool m_isShakeEnabled = false;

    // [PENTING] Director yang mengatur kamera sekarang
    std::unique_ptr<CinematicDirector> m_director;
};