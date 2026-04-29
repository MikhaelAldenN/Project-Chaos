#pragma once

#include <memory>
#include <vector>
#include <string>
#include <imgui.h> // [FIX] Untuk ImVec2 dan fungsionalitas GUI
#include "Scene.h"
#include "Camera.h"
#include "CameraController.h"
#include "Player.h"
#include "Primitive.h"
#include "WindowTrackingSystem.h"
#include "System/PrimitiveRenderer.h"
#include "BeyondWindow.h"

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

    void CloseSubWindowBySDLID(Uint32 sdlWindowID);

private:
    void RenderScene(float elapsedTime, Camera* camera, bool isTransparentWindow);
    void InitializeSubWindows();

    // --- FITUR DEBUG & SYSTEM ---
    void ResetEverything();
    void SpawnDebugWindow();
    void SpawnTransparentWindow();
    void AddLog(const std::string& message);

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

    // --- GUI DEBUG VARIABLES ---
    bool m_showGrid = false;
    float m_timeScale = 1.0f;
    int m_spawnCount = 0;
    bool m_autoSyncMainWindow = true;
    ImVec2 m_debugPanelSize = { 450, 750 }; // Ukuran default panel
    bool m_topmostEnabled = false;

    // Terminal Log System
    std::vector<std::string> m_debugLogs;

    DirectX::XMFLOAT2 m_currentStretch = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_stretchOffset = { 0.0f, 0.0f }; 
    const float m_defaultWinSize = 200.0f;
};