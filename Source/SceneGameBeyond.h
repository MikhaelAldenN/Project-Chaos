#pragma once

#include <memory>
#include <vector>
#include "GameWindow.h"
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 

class SceneGameBeyond : public Scene
{
public:
    SceneGameBeyond();
    ~SceneGameBeyond() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    // 生ポインタを返す (互換性のため)
    Camera* GetMainCamera() const { return mainCamera.get(); }
    Camera* GetSubCamera() const { return subCamera.get(); }

private:
    void RenderScene(float elapsedTime, Camera* camera);

    // --- Update Helpers ---
    void UpdateTrackingWindow();
    void UpdateLensWindow();
    void HandleDebugInput();

    // --- Main Assets (Smart Pointers化) ---
    std::shared_ptr<Camera> mainCamera;
    std::shared_ptr<Camera> subCamera;
    std::unique_ptr<Player> player; // PlayerはUniqueで十分
    std::vector<std::shared_ptr<Camera>> additionalCameras;

    // --- Tracking Window (Auto-follows player) ---
    GameWindow* trackingWindow = nullptr; // WindowManagerが管理するのでRaw Pointerのまま
    std::shared_ptr<Camera> trackingCamera;

    // --- Lens Window (Draggable by user) ---
    GameWindow* lensWindow = nullptr;
    std::shared_ptr<Camera> lensCamera;
};