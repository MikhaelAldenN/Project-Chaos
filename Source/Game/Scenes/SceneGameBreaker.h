#pragma once

// Game Systems
#include <algorithm>
#include <DirectXMath.h>
#include <imgui.h>
#include <memory>
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <wrl/client.h> 
#include "BitmapFont.h"     
#include "CameraController.h"
#include "CinematicDirector.h" 
#include "Framework.h"
#include "Window.h"
#include "GUISceneGameBreaker.h" 
#include "PostProcessManager.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneIntro.h"
#include "System/AudioManager.h"
#include "System/Collision.h"
#include "System/CollisionManager.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "System/Light.h"
#include "UberShader.h"
#include "WindowManager.h"

// Game Objects
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Player.h"
#include "Stage.h"

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
    // CONFIGURATION CONSTANTS
    // =========================================================
    struct Config {
        // Camera
        static constexpr float CAM_FOV = 45.0f;
        static constexpr float CAM_NEAR = 0.1f;
        static constexpr float CAM_FAR = 1000.0f;
        static constexpr float CAM_START_HEIGHT = 20.0f;

        // Post Process / CRT Effect
        static constexpr float FX_CRT_BASE_STRENGTH = 0.2f;
        static constexpr float FX_CRT_ROTATION_TARGET = 0.45f;
        static constexpr float FX_TRANSITION_WINDOW = 0.2f;
        static constexpr float FX_GLITCH_FACTOR = 0.7f;
    };

    // =========================================================
    // INTERNAL HELPER FUNCTIONS
    // =========================================================
    void RenderScene(float elapsedTime, Camera* camera);

    // =========================================================
    // RUNTIME STATE
    // =========================================================
    struct PostProcessState {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableChromatic = true;
        bool EnableCRT = true;
    };

    Player* player = nullptr;
    std::unique_ptr<CollisionManager> m_collisionManager;
    std::unique_ptr<EnemyManager> m_enemyManager;
    std::unique_ptr<ItemManager> m_itemManager;
    std::unique_ptr<Stage> m_stage;
    std::shared_ptr<Camera> mainCamera;

    // Camera Settings
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // Light
    LightManager m_lightManager;

    // Graphics
    std::unique_ptr<PostProcessManager> m_postProcess;
    UberShader::UberData uberParams;
    PostProcessState m_fxState;

    // Visual Config
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // State Variables
    float m_globalTime = 0.0f;
    float m_configFineDensity = 30.0f;
    float m_configZoomDensity = 0.0f;

    std::unique_ptr<CinematicDirector> m_director;
};