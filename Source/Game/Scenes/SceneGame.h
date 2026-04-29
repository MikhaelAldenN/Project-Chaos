#pragma once

// Standard Libraries
#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h> 
#include <PxPhysicsAPI.h> 
#include <SDL3/SDL.h>

#include <vector>
#include "BeyondWindow.h"
#include "Scene.h"
#include "System/Light.h"
#include "UberShader.h"

// ==========================================
// FORWARD DECLARATIONS
// ==========================================
class Camera;
class CinematicDirector;
class CollisionManager;
class EnemyManager;
class GameBreakerGUI;
class ItemManager;
class Player;
class PostProcessManager;
class Stage;

// ==========================================
// CUSTOM DELETERS
// ==========================================
struct PhysXDeleter {
    template <typename T>
    void operator()(T* ptr) const {
        if (ptr) ptr->release();
    }
};

// ==========================================
// MAIN CLASS
// ==========================================
class SceneGame : public Scene
{
    friend class GameBreakerGUI;

public:
    SceneGame();
    ~SceneGame() override;

    SceneGame(const SceneGame&) = delete;
    SceneGame& operator=(const SceneGame&) = delete;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return m_mainCamera.get(); }

private:
    struct Config {
        static constexpr float DEFAULT_SCREEN_W{ 1920.0f };
        static constexpr float DEFAULT_SCREEN_H{ 1080.0f };
        static constexpr float TIME_LOOP_MAX{ 1000.0f };
        static constexpr float GRAVITY{ -9.81f };
        static constexpr float CAM_FOV{ 45.0f };
        static constexpr float CAM_NEAR{ 0.1f };
        static constexpr float CAM_FAR{ 1000.0f };
        static constexpr float CAM_START_HEIGHT{ 20.0f };
        static constexpr float FX_CRT_BASE_STRENGTH{ 0.2f };
        static constexpr float FX_CRT_ROTATION_TARGET{ 0.45f };
        static constexpr float FX_TRANSITION_WINDOW{ 0.2f };
        static constexpr float FX_GLITCH_FACTOR{ 0.7f };
    };

    void RenderScene(float elapsedTime, Camera* camera);

    struct PostProcessState {
        bool MasterEnabled{ true };
        bool EnableVignette{ true };
        bool EnableLens{ true };
        bool EnableChromatic{ true };
        bool EnableCRT{ true };
    };

    std::unique_ptr<Player> m_player{};
    std::unique_ptr<CollisionManager> m_collisionManager{};
    std::unique_ptr<EnemyManager> m_enemyManager{};
    std::unique_ptr<ItemManager> m_itemManager{};
    std::unique_ptr<Stage> m_stage{};
    std::shared_ptr<Camera> m_mainCamera{};

    DirectX::XMFLOAT3 m_cameraPosition{ 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 m_cameraTarget{ 0.0f, 0.0f, 0.0f };
    LightManager m_lightManager{};
    std::unique_ptr<PostProcessManager> m_postProcess{};
    UberShader::UberData m_uberParams{};
    PostProcessState m_fxState{};
    DirectX::XMFLOAT4 m_bgSpriteColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    float m_globalTime{ 0.0f };
    float m_configFineDensity{ 30.0f };
    float m_configZoomDensity{ 0.0f };

    std::unique_ptr<CinematicDirector> m_director{};

    physx::PxDefaultAllocator m_allocator{};
    physx::PxDefaultErrorCallback m_errorCallback{};
    std::unique_ptr<physx::PxFoundation, PhysXDeleter> m_foundation{};
    std::unique_ptr<physx::PxPhysics, PhysXDeleter> m_physics{};
    std::unique_ptr<physx::PxDefaultCpuDispatcher, PhysXDeleter> m_dispatcher{};
    std::unique_ptr<physx::PxScene, PhysXDeleter> m_scene{};
    std::unique_ptr<physx::PxControllerManager, PhysXDeleter> m_controllerManager{};
    std::unique_ptr<physx::PxMaterial, PhysXDeleter> m_defaultMaterial{};
    std::unique_ptr<physx::PxRigidStatic, PhysXDeleter> m_groundPlane{};
};