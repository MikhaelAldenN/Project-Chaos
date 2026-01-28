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

// INCLUDE SYSTEM BARU DI SINI
// System ini sudah membawa definisi TrackedWindow, WindowState, dll.
#include "WindowTrackingSystem.h" 

// =========================================================
// HAPUS BAGIAN INI DARI FILE KAMU (DELETE SEKARANG!)
// =========================================================
/* struct WindowState { ... }  <-- HAPUS
struct TrackedWindowConfig { ... } <-- HAPUS
struct TrackedWindow { ... } <-- HAPUS
*/
// Karena struct di atas sudah ada di dalam WindowTrackingSystem.h

// =========================================================
// TEXT 3D CONFIG (Ini boleh tetap di sini atau dipindah, bebas)
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
// SCENE GAME BEYOND
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

    // SYSTEM
    std::unique_ptr<WindowTrackingSystem> m_windowSystem;

    // States
    bool m_isWindowsInitialized = false;
    bool m_gameStarted = false;
    bool m_shatterTriggered = false;

    // Timers
    float m_startupTimer = 0.0f;
    float m_priorityEnforceTimer = 0.0f;

    // Visuals
    Text3DConfig m_textConfig;
    std::unique_ptr<Primitive> m_primitive2D;
};