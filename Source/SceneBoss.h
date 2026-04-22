#pragma once

#include <memory>
#include "Scene.h"
#include "GameWindow.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h"
#include "Primitive.h"
#include "WindowTrackingSystem.h"
#include "System/PrimitiveRenderer.h"

// =========================================================
// SCENE GAME BEYOND - BASE CODE (WINDOWKILL MODE)
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
    [[nodiscard]] Player* GetPlayer() const { return m_player.get(); }

private:
    void RenderScene(float elapsedTime, Camera* camera);
    void InitializeSubWindows();

private:
    // Core Components
    std::shared_ptr<Camera> m_mainCamera;

    // Game Objects
    std::unique_ptr<Player> m_player;

    // Systems
    std::unique_ptr<WindowTrackingSystem> m_windowSystem;
    std::unique_ptr<Primitive> m_primitive2D;
    std::unique_ptr<PrimitiveRenderer> m_primitive3D;

    // Gameplay Settings
    float m_screenLimitX = 23.0f;
    float m_screenLimitZ = 12.5f;

    // GUI Helpers
    void DrawTabGeneral();
    void DrawTabCamera();
};