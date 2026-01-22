#pragma once

#include <memory>
#include <vector>
#include "GameWindow.h"
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h" 
#include "Boss.h"
#include "BlockManager.h"

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

private:
    void RenderScene(float elapsedTime, Camera* camera);
    void InitializeSubWindows();

    // [BARU] Fungsi Generik untuk menggerakkan Window apapun mengejar Target apapun
    void UpdateWindowTracking(float dt, GameWindow* win, Camera* cam, const DirectX::XMFLOAT3& targetPos, float& currentX, float& currentY);

    // Fungsi spesifik
    void UpdateLensLogic();
    void HandleDebugInput();
    void UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight);
    float GetUnifiedCameraHeight() const;

    // --- Main Assets ---
    std::shared_ptr<Camera> mainCamera;
    std::shared_ptr<Camera> subCamera;
    std::unique_ptr<Player> player;
    std::unique_ptr<Boss> boss;
    std::unique_ptr<BlockManager> blockManager;
    std::vector<std::shared_ptr<Camera>> additionalCameras;

    // --- Windows ---
    GameWindow* trackingWindow = nullptr; // Player Window
    std::shared_ptr<Camera> trackingCamera;

    GameWindow* lensWindow = nullptr;     // Lens Window
    std::shared_ptr<Camera> lensCamera;

    // [BARU] Boss Window
    GameWindow* bossWindow = nullptr;
    std::shared_ptr<Camera> bossCamera;

    bool isWindowsInitialized = false;
    float startupTimer = 0.0f;

    // --- Constants ---
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;
    static constexpr float FIELD_OF_VIEW = 60.0f;
    static constexpr float DEFERRED_INIT_TIME = 0.2f;

    // --- Window Physics State ---
    float m_windowFollowSpeed = 100.0f; // Shared speed setting
    DirectX::XMFLOAT3 m_bossTrackingOffset = { 0.0f, 0.0f, 1.6f };

    // Player Window Position State
    float m_playerWinX = 0.0f;
    float m_playerWinY = 0.0f;

    // [BARU] Boss Window Position State
    float m_bossWinX = 0.0f;
    float m_bossWinY = 0.0f;
};