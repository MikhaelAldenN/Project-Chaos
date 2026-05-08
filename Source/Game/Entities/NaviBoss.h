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
struct ErrorNode {
    DirectX::XMFLOAT2 localOffset;   // Posisi relatif terhadap pusat sayap
    DirectX::XMFLOAT2 targetOffset;  // Titik tujuan saat sayap mekar
    DirectX::XMFLOAT2 size;          // Ukuran kotak
    float flapOffset;                // Jeda gelombang animasi
};

class NaviBoss {
public:
    NaviBoss();
    ~NaviBoss();

    void Initialize(WindowTrackingSystem* windowSystem);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, Camera* currentCamera);

    void SetBreathParams(float speed, float intensity) {
        m_breathSpeed = speed;
        m_breathIntensity = intensity;
    }
    float GetBreathSpeed() const { return m_breathSpeed; }
    float GetBreathIntensity() const { return m_breathIntensity; }

    void SetWingParams(float speed, float intensity) {
        m_wingFlapSpeed = speed;
        m_wingFlapIntensity = intensity;
    }
    float GetWingFlapSpeed() const { return m_wingFlapSpeed; }
    float GetWingFlapIntensity() const { return m_wingFlapIntensity; }
    
    void SetWingOffsets(float xOffset, float zOffset) {
        m_wingXOffset = xOffset;
        m_wingZOffset = zOffset;
    }
    float GetWingXOffset() const { return m_wingXOffset; }
    float GetWingZOffset() const { return m_wingZOffset; } // Tambahan untuk vertikal

    Camera* GetLeftWingCamera() const { return m_leftWingCamera.get(); }
    Camera* GetRightWingCamera() const { return m_rightWingCamera.get(); }



    void SetScalingParams(float pixelToUnit, float globalScale) {
        m_pixelToUnit = pixelToUnit;
        m_wingGlobalScale = globalScale;
    }
    float GetPixelToUnit() const { return m_pixelToUnit; }
    float GetWingGlobalScale() const { return m_wingGlobalScale; }

    Beyond::Window* GetMainWindow() const { return m_naviWindow; }
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

private:
    void GenerateButterflyWings();

    enum class WingState {
        Expanding,
        Idle
    };
    WingState m_wingState = WingState::Expanding;
    float m_wingStateTimer = 0.0f;
    const float WING_EXPAND_DURATION = 2.0f;

private:
    // --- KOMPONEN KEPALA ---
    Beyond::Window* m_naviWindow = nullptr;
    std::shared_ptr<Camera> m_naviCamera;
    std::unique_ptr<Sprite> m_faceSprite;

    // --- KOMPONEN SAYAP (BARU) ---
    Beyond::Window* m_leftWingWindow = nullptr;
    std::shared_ptr<Camera> m_leftWingCamera;

    Beyond::Window* m_rightWingWindow = nullptr;
    std::shared_ptr<Camera> m_rightWingCamera;

    std::unique_ptr<Sprite> m_wingSprite; // Gambar 1 kotak untuk tes
    std::vector<ErrorNode> m_leftWingData;
    std::vector<ErrorNode> m_rightWingData;

    DirectX::XMFLOAT2 m_wingWindowSize = { 1000.0f, 1000.0f }; // Ukuran kanvas sayap


    // --- TRANSFORM DATA ---
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 5.0f };
    DirectX::XMFLOAT2 m_baseWindowSize = { 200.0f, 200.0f };
    DirectX::XMFLOAT2 m_windowSize = { 400.0f, 400.0f };

    float m_breathSpeed = 2.0f;
    float m_breathIntensity = 14.0f;
    float m_glitchTimer = 0.0f;

    float m_wingXOffset = 4.2f;       // Jarak horizontal (Kiri/Kanan)
    float m_wingZOffset = -3.3f;       // Jarak vertikal di layar (Atas/Bawah)
    float m_wingFlapSpeed = 0.7f;      // Kecepatan kepakan
    float m_wingFlapIntensity = 0.02f;  // Seberapa jauh sayap bergerak saat mengepak

    DirectX::XMFLOAT2 m_wingNodeBaseSize = { 50.0f, 50.0f };

    // Wing Scaling Config
    float m_pixelToUnit = 40.0f;      // Pengganti hardcode /40.0f
    float m_wingGlobalScale = 2.5f;   // Pengali skala akhir

};