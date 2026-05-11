#pragma once
#include "INaviPhase.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "BeyondWindow.h"
#include "Camera.h"

class Sprite;

// Pindahkan struktur sayap ke sini, karena hanya Fase Windowkill yang butuh ini!
struct WingNode {
    DirectX::XMFLOAT2 localOffset;
    DirectX::XMFLOAT2 targetOffset;
    DirectX::XMFLOAT2 size;
    float flapOffset;
    float spawnDelay = 0.0f;
    float animScale = 0.0f;
    bool isClosing = false;
};

class NaviPhaseWindowkill : public INaviPhase {
public:
    NaviPhaseWindowkill();
    ~NaviPhaseWindowkill() override = default;

    // --- Kontrak Wajib dari INaviPhase ---
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // --- Custom Functions untuk Fase Ini (Dipanggil oleh ImGui) ---
    void ReplayAnimation();

    void SetWingFlapParams(float speed, float intensity) { m_wingFlapSpeed = speed; m_wingFlapIntensity = intensity; }
    float GetWingFlapSpeed() const { return m_wingFlapSpeed; }
    float GetWingFlapIntensity() const { return m_wingFlapIntensity; }

    void SetWingOffsets(float xOffset, float zOffset) { m_wingXOffset = xOffset; m_wingZOffset = zOffset; }
    float GetWingOffsetX() const { return m_wingXOffset; }
    float GetWingOffsetZ() const { return m_wingZOffset; }

    void SetWingSeed(unsigned int seed) {
        if (m_wingSeed != seed) { m_wingSeed = seed; GenerateButterflyWings(); }
    }
    unsigned int GetWingSeed() const { return m_wingSeed; }

    void SetSpawnParams(float popDur, float spawnDur, float chaos) {
        m_popDuration = popDur; m_spawnDuration = spawnDur; m_spawnChaos = chaos;
    }
    float GetPopDuration() const { return m_popDuration; }
    float GetSpawnDuration() const { return m_spawnDuration; }
    float GetSpawnChaos() const { return m_spawnChaos; }

    void SetScalingParams(float p2u, float scale) { m_pixelToUnit = p2u; m_wingGlobalScale = scale; }
    float GetPixelToUnit() const { return m_pixelToUnit; }
    float GetWingGlobalScale() const { return m_wingGlobalScale; }

    void SetFXClickThrough(bool enable) { if (m_fxWindow) m_fxWindow->SetClickThrough(enable); }
    bool IsFXClickThrough() const { return m_fxWindow ? m_fxWindow->IsClickThrough() : true; }
    Camera* GetFXCamera() const { return m_fxCamera.get(); }

private:
    void GenerateButterflyWings();

private:
    // Komponen FX
    Beyond::Window* m_fxWindow = nullptr;
    std::shared_ptr<Camera> m_fxCamera;
    std::unique_ptr<Sprite> m_wingSprite;

    std::vector<WingNode> m_leftWingData;
    std::vector<WingNode> m_rightWingData;

    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    // State Animasi
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

    float m_glitchTimer = 0.0f;
    float m_wingXOffset = 4.2f;
    float m_wingZOffset = -0.856f;
    float m_wingFlapSpeed = 0.7f;
    float m_wingFlapIntensity = 0.02f;

    float m_pixelToUnit = 40.0f;
    float m_wingGlobalScale = 2.5f;
};