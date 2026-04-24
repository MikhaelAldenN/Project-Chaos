#pragma once

#include <memory>
#include <vector>
#include "GameWindow.h"
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 

class SceneGame : public Scene
{
public:
    SceneGame();
    ~SceneGame() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return mainCamera.get(); }
    Camera* GetSubCamera() const { return subCamera; }

private:
    void RenderScene(float elapsedTime, Camera* camera);

    // --- Update Helpers ---
    void UpdateTrackingWindow();
    void UpdateLensWindow();
    void HandleDebugInput();

    // --- Main Assets ---
    std::shared_ptr<Camera> mainCamera;
    Camera* subCamera = nullptr;
    Player* player = nullptr;
    std::vector<Camera*> additionalCameras;

    // --- Tracking Window (Auto-follows player) ---
    GameWindow* trackingWindow = nullptr;
    Camera* trackingCamera = nullptr;

    // --- Lens Window (Draggable by user) ---
    GameWindow* lensWindow = nullptr;
    Camera* lensCamera = nullptr;
};