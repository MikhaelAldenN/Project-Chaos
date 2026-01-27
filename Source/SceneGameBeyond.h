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

// =========================================================
// WINDOW STATE - Track position untuk smooth movement
// =========================================================
struct WindowState
{
    int actualX = 0;
    int actualY = 0;
    float targetX = 0.0f;
    float targetY = 0.0f;
};

// =========================================================
// TRACKED WINDOW CONFIG - Data untuk init window
// =========================================================
struct TrackedWindowConfig
{
    std::string name;              // Identifier (e.g., "player", "monitor1", "cpu")
    std::string title;             // Window title
    int width = 300;
    int height = 300;
    int priority = 0;

    DirectX::XMFLOAT3 trackingOffset = { 0.0f, 0.0f, 0.0f };
};

// =========================================================
// TRACKED WINDOW - Single window instance
// =========================================================
struct TrackedWindow
{
    std::string name;
    GameWindow* window = nullptr;
    std::shared_ptr<Camera> camera;
    WindowState state;
    DirectX::XMFLOAT3 trackingOffset = { 0.0f, 0.0f, 0.0f };

    // Function pointer untuk get target position
    // Bisa point ke Player, Boss Part, atau custom logic
    std::function<DirectX::XMFLOAT3()> getTargetPositionFunc;
};

// =========================================================
// TEXT 3D CONFIG
// =========================================================
struct Text3DConfig
{
    char label[128] = ">C:\\_";
    DirectX::XMFLOAT3 offset = { -2.3f, 3.8f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 68.0f, 0.0f, 0.0f };
    float scale = 0.011f;
    DirectX::XMFLOAT4 color = { 237.0f / 255.0f, 192.0f / 255.0f, 17.0f / 255.0f, 1.0f };
};

// =========================================================
// SCENE GAME BEYOND - Refactored
// =========================================================
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

    // Window Management (SIMPLIFIED!)
    bool AddTrackedWindow(const TrackedWindowConfig& config,
        std::function<DirectX::XMFLOAT3()> getTargetPos);
    void UpdateAllTrackedWindows(float dt);
    void UpdateSingleWindow(float dt, TrackedWindow& tracked);

    TrackedWindow* GetTrackedWindow(const std::string& name);

    // Camera Utilities
    void UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight);

    // Screen Math Helpers
    void WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY) const;
    void GetScreenDimensions(int& outWidth, int& outHeight) const;
    [[nodiscard]] float GetUnifiedCameraHeight() const;

private:
    // Constants
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;
    static constexpr float FIELD_OF_VIEW = 60.0f;
    static constexpr float DEFERRED_INIT_TIME = 0.2f;
    static constexpr float INTRO_DELAY = 0.5f;

    // Cache
    mutable int m_cachedScreenWidth = 0;
    mutable int m_cachedScreenHeight = 0;
    mutable float m_cacheUpdateTimer = 0.0f;
    static constexpr float CACHE_REFRESH_INTERVAL = 1.0f;

    // Assets & Core Cameras
    std::shared_ptr<Camera> m_mainCamera;
    std::shared_ptr<Camera> m_subCamera;
    std::vector<std::shared_ptr<Camera>> m_additionalCameras;

    std::unique_ptr<Player> m_player;
    std::unique_ptr<Boss> m_boss;
    std::unique_ptr<BlockManager> m_blockManager;

    // =====================================================
    // TRACKED WINDOWS - All windows managed here!
    // =====================================================
    std::vector<std::unique_ptr<TrackedWindow>> m_trackedWindows;
    std::unordered_map<std::string, TrackedWindow*> m_windowLookup;

    // States & Settings
    bool m_isWindowsInitialized = false;
    float m_startupTimer = 0.0f;
    float m_windowFollowSpeed = 100.0f;

    // Priority Enforcement Throttle
    float m_priorityEnforceTimer = 0.0f;
    static constexpr float PRIORITY_ENFORCE_INTERVAL = 0.5f;

    // Intro & Effects
    bool m_shatterTriggered = false;
    float m_introTimer = 0.0f;
    bool m_gameStarted = false;

    // UI/Text
    Text3DConfig m_textConfig;
    std::unique_ptr<Primitive> m_primitive2D;
};