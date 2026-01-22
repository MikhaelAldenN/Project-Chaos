#pragma once

#include <memory>
#include <vector>
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 
#include "GameWindow.h"

class SceneGameBeyond : public Scene
{
public:
    SceneGameBeyond();
    ~SceneGameBeyond() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    // Getters for raw pointers (compatibility)
    Camera* GetMainCamera() const { return mainCamera.get(); }
    Camera* GetSubCamera() const { return subCamera.get(); }

private:
    // --- Constants ---
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f; // 40 pixels = 1 Unity Unit
    static constexpr float FIELD_OF_VIEW = 60.0f;       // Vertical FOV in degrees
    static constexpr float DEFERRED_INIT_TIME = 0.2f;   // Seconds to wait before spawning windows

private:
    // --- Internal Helpers ---
    void InitializeSubWindows();
    void UpdateTrackingLogic();
    void UpdateLensLogic();
    void HandleDebugInput();
    void RenderScene(float elapsedTime, Camera* camera);

    // Core math to sync window view with world position (DRY implementation)
    void UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight);

    // Calculates the Y-height required to match 1:1 pixel mapping
    float GetUnifiedCameraHeight() const;

private:
    // --- Components ---
    std::shared_ptr<Camera> mainCamera;
    std::shared_ptr<Camera> subCamera;
    std::unique_ptr<Player> player;
    std::vector<std::shared_ptr<Camera>> additionalCameras;

    // --- Sub-Windows (Owned by WindowManager) ---
    GameWindow* trackingWindow = nullptr;
    std::shared_ptr<Camera> trackingCamera;

    GameWindow* lensWindow = nullptr;
    std::shared_ptr<Camera> lensCamera;

    // --- State ---
    bool isWindowsInitialized = false;
    float startupTimer = 0.0f;
};