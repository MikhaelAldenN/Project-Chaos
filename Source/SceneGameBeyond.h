#pragma once

#include <memory>
#include <vector>
#include <string>
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

// Grouped settings for 3D Text rendering
struct Text3DConfig
{
    char label[128] = ">C:\\_";
    DirectX::XMFLOAT3 offset = { -2.3f, 3.8f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 68.0f, 0.0f, 0.0f };
    float scale = 0.011f;
    DirectX::XMFLOAT4 color = { 237.0f / 255.0f, 192.0f / 255.0f, 17.0f / 255.0f, 1.0f };
};

// [BARU] Anti-jitter: Track actual window positions
struct WindowState
{
    int actualX = 0;      // Posisi actual dari SDL (integer)
    int actualY = 0;
    float targetX = 0.0f; // Target smooth position (float precision)
    float targetY = 0.0f;
};

class SceneGameBeyond : public Scene
{
public:
    SceneGameBeyond();
    ~SceneGameBeyond() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    [[nodiscard]] Camera* GetMainCamera() const { return m_mainCamera.get(); }

private:
    // Core Functions
    void RenderScene(float elapsedTime, Camera* camera);
    void InitializeSubWindows();
    void HandleDebugInput();

    // Window & Camera Utilities (SIGNATURE BERUBAH!)
    void UpdateWindowTracking(float dt, GameWindow* win, Camera* cam,
    const DirectX::XMFLOAT3& targetPos, WindowState& winState);
    void UpdateLensLogic();
    void UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight);

    // Screen Math Helpers (DRY implementations)
    void WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY) const;
    void GetScreenDimensions(int& outWidth, int& outHeight) const;
    [[nodiscard]] float GetUnifiedCameraHeight() const;

private:
    // Constants
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;
    static constexpr float FIELD_OF_VIEW = 60.0f;
    static constexpr float DEFERRED_INIT_TIME = 0.2f;
    static constexpr float INTRO_DELAY = 0.5f;

    mutable int m_cachedScreenWidth = 0;
    mutable int m_cachedScreenHeight = 0;
    mutable float m_cacheUpdateTimer = 0.0f;
    static constexpr float CACHE_REFRESH_INTERVAL = 1.0f; // Update tiap 1 detik

    // Assets & Core Cameras
    std::shared_ptr<Camera> m_mainCamera;
    std::shared_ptr<Camera> m_subCamera;
    std::vector<std::shared_ptr<Camera>> m_additionalCameras;

    std::unique_ptr<Player> m_player;
    std::unique_ptr<Boss> m_boss;
    std::unique_ptr<BlockManager> m_blockManager;

    // Windows & Associated Cameras
    GameWindow* m_trackingWindow = nullptr;
    std::shared_ptr<Camera> m_trackingCamera;

    GameWindow* m_lensWindow = nullptr;
    std::shared_ptr<Camera> m_lensCamera;

    GameWindow* m_bossWindow = nullptr;
    std::shared_ptr<Camera> m_bossCamera;

    GameWindow* m_cpuWindow = nullptr;
    std::shared_ptr<Camera> m_cpuCamera;

    GameWindow* m_monitor2Window = nullptr;
    std::shared_ptr<Camera> m_monitor2Camera;

    // States & Settings
    bool m_isWindowsInitialized = false;
    float m_startupTimer = 0.0f;

    float m_windowFollowSpeed = 100.0f;
    DirectX::XMFLOAT3 m_bossTrackingOffset = { -0.3f, 0.0f, 2.1f };
    DirectX::XMFLOAT3 m_cpuTrackingOffset = { -8.2f, 0.0f, 4.0f };

    int m_bossWindowSize[2] = { 340, 340 };
    int m_cpuWindowSize[2] = { 186, 370 };

    DirectX::XMFLOAT3 m_monitor2TrackingOffset = { 0.5f, 0.0f, -0.3f }; // Default offset
    int m_monitor2WindowSize[2] = { 240, 210 }; // Default size

    // [GANTI DARI WindowPos KE WindowState]
    WindowState m_playerWindowState;
    WindowState m_bossWindowState;
    WindowState m_cpuWindowState;
    WindowState m_monitor2WindowState;

    // Intro & Effects
    bool m_shatterTriggered = false;
    float m_introTimer = 0.0f;

    // UI/Text
    Text3DConfig m_textConfig;
    std::unique_ptr<Primitive> m_primitive2D;


    bool m_gameStarted = false;

    float m_priorityEnforceTimer = 0.0f;
    static constexpr float PRIORITY_ENFORCE_INTERVAL = 0.5f;
};