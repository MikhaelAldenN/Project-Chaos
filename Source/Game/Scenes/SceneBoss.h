#pragma once

#include <memory>
#include <vector>
#include <string>
#include <imgui.h>
#include <PxPhysicsAPI.h>

#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h"
#include "Primitive.h"
#include "WindowTrackingSystem.h"
#include "System/PrimitiveRenderer.h"
#include "BeyondWindow.h"
#include "PhysXUtils.h"

// =========================================================
// SCENE BOSS - WINDOWKILL MODE
// =========================================================

class SceneBoss : public Scene
{
public:
    SceneBoss();
    ~SceneBoss() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    [[nodiscard]] Camera* GetMainCamera() const { return m_mainCamera.get(); }
    [[nodiscard]] Player* GetPlayer()     const { return m_player.get(); }

    void CloseSubWindowBySDLID(Uint32 sdlWindowID);

private:
    // --- Render Pipeline ---
    void RenderScene(float elapsedTime, Camera* camera, bool isTransparentWindow);

    // --- Initialization ---
    void InitializePhysics();
    void InitializeSubWindows();

    // --- Debug & System Helpers ---
    void ResetEverything();
    void SpawnDebugWindow();
    void SpawnTransparentWindow(float bgAlpha, const std::string& typeSuffix);
    void AddLog(const std::string& message);

private:
    // =========================================================
    // LOCAL CONSTANTS
    // =========================================================
    static constexpr float k_fov = 60.0f;
    static constexpr float k_pixelToUnitRatio = 40.0f;
    static constexpr float k_camNear = 0.1f;
    static constexpr float k_camFar = 1000.0f;
    static constexpr float k_defaultWinSize = 200.0f;
    //static constexpr float k_screenLimitX = 23.0f;
    //static constexpr float k_screenLimitZ = 12.5f;

    // =========================================================
    // CORE COMPONENTS
    // =========================================================
    std::shared_ptr<Camera>               m_mainCamera;
    std::unique_ptr<Player>               m_player;
    std::unique_ptr<WindowTrackingSystem> m_windowSystem;
    std::unique_ptr<Primitive>            m_primitive2D;
    std::unique_ptr<PrimitiveRenderer>    m_primitive3D;

    // =========================================================
    // PHYSX (minimal — no ground plane, no gravity)
    // =========================================================
    physx::PxDefaultAllocator                                   m_allocator;
    physx::PxDefaultErrorCallback                               m_errorCallback;
    std::unique_ptr<physx::PxFoundation, PhysXDeleter>  m_foundation;
    std::unique_ptr<physx::PxPhysics, PhysXDeleter>  m_physics;
    std::unique_ptr<physx::PxDefaultCpuDispatcher, PhysXDeleter> m_dispatcher;
    std::unique_ptr<physx::PxScene, PhysXDeleter>  m_scene;
    std::unique_ptr<physx::PxControllerManager, PhysXDeleter>  m_controllerManager;
    std::unique_ptr<physx::PxMaterial, PhysXDeleter>  m_defaultMaterial;

    // =========================================================
    // SQUASH & STRETCH STATE
    // =========================================================
    DirectX::XMFLOAT2 m_currentStretch = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_stretchOffset = { 0.0f, 0.0f };

    // =========================================================
    // GUI / DEBUG STATE
    // =========================================================
    bool      m_showGrid = false;
    float     m_timeScale = 1.0f;
    int       m_spawnCount = 0;
    bool      m_autoSyncMainWindow = true;
    bool      m_topmostEnabled = true;
    bool      m_playerWindowTransparent = false;
    ImVec2    m_debugPanelSize = { 450.0f, 750.0f };

    // Terminal log (capped at 50 lines)
    std::vector<std::string> m_debugLogs;
};