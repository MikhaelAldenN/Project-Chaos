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
#include "BitmapFont.h"
#include "ResourceManager.h"
#include "WindowShatter.h"

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

    // --- 3D TEXT EDITOR DATA ---
    // Gunakan char array biar bisa diedit pakai ImGui::InputText
// --- 3D TEXT EDITOR DATA ---

    // Label Content: >C:\_ (Backslash harus double)
    char m_textLabel[128] = ">C:\\_";

    // Offset Position: -2.3, 3.8, 0.0
    DirectX::XMFLOAT3 m_textOffset = { -2.3f, 3.8f, 0.0f };

    // Rotation: 68, 0, 0
    DirectX::XMFLOAT3 m_textRotation = { 68.0f, 0.0f, 0.0f };

    // Scale: 0.0110
    float m_textScale = 0.011f;

    // Text Color (R:237 G:192 B:17) -> Dikonversi ke Float
    DirectX::XMFLOAT4 m_textColor = { 237.0f / 255.0f, 192.0f / 255.0f, 17.0f / 255.0f, 1.0f };

    bool m_shatterSpawned = false;  // Flag agar hanya spawn sekali
    float m_shatterDelay = 0.3f;    // Delay sebelum spawn (opsional)
    float m_shatterTimer = 0.0f;
};