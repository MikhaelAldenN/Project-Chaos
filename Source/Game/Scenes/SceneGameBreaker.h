#pragma once

// Game Systems
#include <algorithm>
#include <cassert>
#include <DirectXMath.h>
#include <imgui.h>
#include <memory>
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <wrl/client.h> 
#include <PxPhysicsAPI.h> 
#include "BitmapFont.h"     
#include "CameraController.h"
#include "CinematicDirector.h" 
#include "Framework.h"
#include "GameWindow.h"
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
    struct Config {
        static constexpr float CAM_FOV = 45.0f;
        static constexpr float CAM_NEAR = 0.1f;
        static constexpr float CAM_FAR = 1000.0f;
        static constexpr float CAM_START_HEIGHT = 20.0f;
        static constexpr float FX_CRT_BASE_STRENGTH = 0.2f;
        static constexpr float FX_CRT_ROTATION_TARGET = 0.45f;
        static constexpr float FX_TRANSITION_WINDOW = 0.2f;
        static constexpr float FX_GLITCH_FACTOR = 0.7f;
    };

    void RenderScene(float elapsedTime, Camera* camera);

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

    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };
    LightManager m_lightManager;
    std::unique_ptr<PostProcessManager> m_postProcess;
    UberShader::UberData uberParams;
    PostProcessState m_fxState;
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float m_globalTime = 0.0f;
    float m_configFineDensity = 30.0f;
    float m_configZoomDensity = 0.0f;

    std::unique_ptr<CinematicDirector> m_director;

    physx::PxDefaultAllocator m_allocator;
    physx::PxDefaultErrorCallback m_errorCallback;
    physx::PxFoundation* m_foundation = nullptr;
    physx::PxPhysics* m_physics = nullptr;
    physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
    physx::PxScene* m_scene = nullptr;
    physx::PxControllerManager* m_controllerManager = nullptr;
    physx::PxMaterial* m_defaultMaterial = nullptr;
    physx::PxRigidStatic* m_groundPlane = nullptr;
};