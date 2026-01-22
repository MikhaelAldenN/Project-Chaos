#pragma once

#include <memory>
#include <vector>
#include "GameWindow.h"
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 
#include "Boss.h"
#include "BlockManager.h" // [1] Tambahkan Include ini

class SceneGameBeyond : public Scene
{
public:
    SceneGameBeyond();
    ~SceneGameBeyond() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return mainCamera.get(); }
    Camera* GetSubCamera() const { return subCamera.get(); }

private:
    void RenderScene(float elapsedTime, Camera* camera);

    // ... (Helper functions lain tetap sama) ...
    void UpdateTrackingLogic(float dt);
    void UpdateLensLogic();
    void HandleDebugInput();
    void UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight);
    float GetUnifiedCameraHeight() const;
    void InitializeSubWindows();

    // --- Main Assets ---
    std::shared_ptr<Camera> mainCamera;
    std::shared_ptr<Camera> subCamera;
    std::unique_ptr<Player> player;
    std::unique_ptr<Boss> boss;

    // [2] Tambahkan BlockManager
    std::unique_ptr<BlockManager> blockManager;

    std::vector<std::shared_ptr<Camera>> additionalCameras;

    // ... (Variable window & deferred init tetap sama) ...
    GameWindow* trackingWindow = nullptr;
    std::shared_ptr<Camera> trackingCamera;

    GameWindow* lensWindow = nullptr;
    std::shared_ptr<Camera> lensCamera;

    bool isWindowsInitialized = false;
    float startupTimer = 0.0f;

    // Constants
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;
    static constexpr float FIELD_OF_VIEW = 60.0f;
    static constexpr float DEFERRED_INIT_TIME = 0.2f;

    // [BARU] Window Smoothing State
    float m_windowX = 0.0f;
    float m_windowY = 0.0f;

    // Semakin KECIL = Semakin Lambat/Smooth (Misal 2.0f)
    // Semakin BESAR = Semakin Responsif/Nempel (Misal 15.0f)
    float m_windowFollowSpeed = 100.0f;
};