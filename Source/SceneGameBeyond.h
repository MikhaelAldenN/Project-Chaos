#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include "Scene.h"
#include "GameWindow.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 
#include "Boss.h"
#include "BlockManager.h"
#include "BitmapFont.h"
#include "ResourceManager.h"
#include "WindowShatter.h"
#include "Primitive.h"
#include "WindowTrackingSystem.h"
#include "EnemyManager.h"

// =========================================================
// SCENE GAME BEYOND - NO ImGui VERSION
// =========================================================
class SceneGameBeyond : public Scene
{
public:
    SceneGameBeyond();
    ~SceneGameBeyond() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override; // Empty implementation - no ImGui
    void OnResize(int width, int height) override;

    [[nodiscard]] Camera* GetMainCamera() const { return m_mainCamera.get(); }

private:
    void RenderScene(float elapsedTime, Camera* camera);
    void InitializeSubWindows();
    void HandleDebugInput();

private:
    // Core Components
    std::shared_ptr<Camera> m_mainCamera;
    std::shared_ptr<Camera> m_subCamera;
    std::vector<std::shared_ptr<Camera>> m_additionalCameras;

    // Game Objects
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Boss> m_boss;
    std::unique_ptr<BlockManager> m_blockManager;

    // Window System
    std::unique_ptr<WindowTrackingSystem> m_windowSystem;

    std::unique_ptr<EnemyManager> m_enemyManager;

    // States
    bool m_isWindowsInitialized = false;
    bool m_gameStarted = false;
    bool m_shatterTriggered = false;

    // Timers
    float m_startupTimer = 0.0f;
    float m_priorityEnforceTimer = 0.0f;

    // Visuals
    std::unique_ptr<Primitive> m_primitive2D;

    DirectX::XMFLOAT3 GetMouseOnGround(Camera* camera);
};