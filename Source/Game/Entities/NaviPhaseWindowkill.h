#pragma once
#include "INaviPhase.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "BeyondWindow.h"
#include "Camera.h"
#include "Bullet.h"
#include "Primitive.h"

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

struct BouncingWindowBullet {
    std::unique_ptr<Bullet> bullet;
    std::string windowName;
    int bounceCount = 0;
    int maxBounces = 8;
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

    void TriggerBouncingWindows(NaviBoss* boss);
    std::vector<Bullet*> GetProjectiles();

    void TriggerOrbitalBlaster(NaviBoss* boss);

    struct OrbitalBlaster {
        bool active = false;
        int state = 0;
        float timer = 0.0f;

        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 targetPos = { 0.0f, 0.0f, 0.0f };

        float beamScaleX = 0.0f;
        // HAPUS "float headScale = 2.0f;" dari sini!

        std::string windowName = "blaster_cannon_window";
        std::string beamWindowName = "blaster_beam_window";
        float beamCurrentLength = 0.0f;
    };

    OrbitalBlaster GetBlaster() const { return m_testBlaster; }

    // ==========================================
    // PARAMETER ORBITAL BLASTER (REFINED)
    // ==========================================
    struct BlasterParams {
        // Pemisahan Cannon (Kepala Meriam)
        float cannonWindowSize = 200.0f;  // Ukuran Jendela OS Cannon
        float cannonVisualScale = 2.0f;   // Ukuran Model 3D Cannon
        float cannonHitboxRadius = 1.5f;  // Hitbox jika player nabrak meriamnya

        // Pemisahan Laser Beam
        float beamVisualWidth = 2.0f;     // Lebar visual energi 2D (Cyan)
        float beamHitboxWidth = 5.0f;     // Lebar hitbox damage (kawat merah)
        float beamMaxLength = 120.0f;     // Panjang maksimal laser

        float beamGrowSpeed = 20.0f;
        float beamSlideSpeed = 1.0f;
        int   beamDamage = 20;
    };

    // ==========================================
    // PARAMETER WINDOW MEMANTUL (REFINED)
    // ==========================================
    struct BouncingBulletParams {
        float speed = 25.0f;
        int   maxBounces = 8;
        int   spawnCount = 3;

        // Pemisahan Ukuran Jendela OS
        float windowWidth = 230.0f;
        float windowHeight = 230.0f;

        // Pemisahan Visual vs Hitbox
        float visualScale = 12.0f;    // Ukuran model 3D (bola)
        float hitboxRadius = 2.0f;   // Radius deteksi tabrakan fisik

        int   damage = 10;
    };

    BlasterParams& GetBlasterParams() { return m_blasterParams; }
    BouncingBulletParams& GetBouncingParams() { return m_bouncingParams; }
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
    
    //---- bullets ------
    std::vector<BouncingWindowBullet> m_bouncingBullets;
    int m_bounceCounter = 0;

    std::shared_ptr<Model> m_placeholderModel;
    OrbitalBlaster m_testBlaster; // Untuk dicoba 1 dulu

    BlasterParams m_blasterParams; // Instance parameter
    BouncingBulletParams m_bouncingParams; // Instance parameter

    std::unique_ptr<Primitive> m_solidRenderer;
};