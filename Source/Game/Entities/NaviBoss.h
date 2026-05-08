#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include "BeyondWindow.h"
#include "Camera.h"

class WindowTrackingSystem;
class Sprite;

// --- STRUKTUR DATA SAYAP ---
struct WingNode {
    DirectX::XMFLOAT2 localOffset;
    DirectX::XMFLOAT2 targetOffset;
    DirectX::XMFLOAT2 size;
    float flapOffset;
    float spawnDelay = 0.0f;
    float animScale = 0.0f;
    bool isClosing = false;
};

class NaviBoss {
public:
    NaviBoss();
    ~NaviBoss();

    void Initialize(WindowTrackingSystem* windowSystem);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, Camera* currentCamera);

    // Fungsi untuk ImGui Replay
    void ReplayAnimation();

    // --- Core Kinematics ---
    void SetCoreBreathParams(float speed, float intensity) {
        m_breathSpeed = speed;
        m_breathIntensity = intensity;
    }
    float GetCoreBreathSpeed() const { return m_breathSpeed; }
    float GetCoreBreathIntensity() const { return m_breathIntensity; }

    // --- Wing Kinematics ---
    void SetWingFlapParams(float speed, float intensity) {
        m_wingFlapSpeed = speed;
        m_wingFlapIntensity = intensity;
    }
    float GetWingFlapSpeed() const { return m_wingFlapSpeed; }
    float GetWingFlapIntensity() const { return m_wingFlapIntensity; }

    void SetWingOffsets(float xOffset, float zOffset) {
        m_wingXOffset = xOffset;
        m_wingZOffset = zOffset;
    }
    float GetWingOffsetX() const { return m_wingXOffset; }
    float GetWingOffsetZ() const { return m_wingZOffset; }

    // --- Procedural Generation & Animation ---
    void SetWingSeed(unsigned int seed) {
        if (m_wingSeed != seed) {
            m_wingSeed = seed;
            GenerateButterflyWings();
        }
    }
    unsigned int GetWingSeed() const { return m_wingSeed; }

    void SetSpawnParams(float popDuration, float spawnDuration, float spawnChaos) {
        m_popDuration = popDuration;
        m_spawnDuration = spawnDuration;
        m_spawnChaos = spawnChaos;
    }
    float GetPopDuration() const { return m_popDuration; }
    float GetSpawnDuration() const { return m_spawnDuration; }
    float GetSpawnChaos() const { return m_spawnChaos; }

    // --- Render Scaling ---
    void SetScalingParams(float pixelToUnit, float globalScale) {
        m_pixelToUnit = pixelToUnit;
        m_wingGlobalScale = globalScale;
    }
    float GetPixelToUnit() const { return m_pixelToUnit; }
    float GetWingGlobalScale() const { return m_wingGlobalScale; }

    // --- System Accessors ---
    Camera* GetFXCamera() const { return m_fxCamera.get(); }
    Beyond::Window* GetMainWindow() const { return m_naviWindow; }
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

    // --- Global FX Controls ---
    void SetFXClickThrough(bool enable) {
        if (m_fxWindow) m_fxWindow->SetClickThrough(enable);
    }
    bool IsFXClickThrough() const {
        return m_fxWindow ? m_fxWindow->IsClickThrough() : true;
    }

private:
    void GenerateButterflyWings();

    enum class WingState { Expanding, Idle };
    WingState m_wingState = WingState::Expanding;
    float m_wingStateTimer = 0.0f;
    const float WING_EXPAND_DURATION = 2.0f;

    unsigned int m_wingSeed = 1337;
    float m_wingFlickerTimer = 0.0f;
    float m_nextFlickerTarget = 0.2f;

    float m_popDuration = 0.15f;
    float m_spawnDuration = 1.5f;
    float m_spawnChaos = 0.5f;

private:
    // --- KOMPONEN KEPALA ---
    Beyond::Window* m_naviWindow = nullptr;
    std::shared_ptr<Camera> m_naviCamera;
    std::unique_ptr<Sprite> m_faceSprite;

    // --- KOMPONEN GLOBAL FX (FULLSCREEN) ---
    Beyond::Window* m_fxWindow = nullptr;
    std::shared_ptr<Camera> m_fxCamera;
    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    std::unique_ptr<Sprite> m_wingSprite;
    std::vector<WingNode> m_leftWingData;
    std::vector<WingNode> m_rightWingData;

    // --- TRANSFORM DATA ---
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 5.0f };
    DirectX::XMFLOAT2 m_baseWindowSize = { 200.0f, 200.0f };
    DirectX::XMFLOAT2 m_windowSize = { 400.0f, 400.0f };

    float m_breathSpeed = 2.0f;
    float m_breathIntensity = 14.0f;
    float m_glitchTimer = 0.0f;

    float m_wingXOffset = 4.2f;
    float m_wingZOffset = -0.856f;
    float m_wingFlapSpeed = 0.7f;
    float m_wingFlapIntensity = 0.02f;

    DirectX::XMFLOAT2 m_wingNodeBaseSize = { 50.0f, 50.0f };
    float m_pixelToUnit = 40.0f;
    float m_wingGlobalScale = 2.5f;
};