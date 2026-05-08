#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include "BeyondWindow.h"
#include "Camera.h"

class WindowTrackingSystem;
class Sprite;

class NaviBoss {
public:
    NaviBoss();
    ~NaviBoss();

    // Step 1: Inisialisasi Window Utama
    void Initialize(WindowTrackingSystem* windowSystem);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, Camera* currentCamera);

    void SetBreathParams(float speed, float intensity) {
        m_breathSpeed = speed;
        m_breathIntensity = intensity;
    }
    float GetBreathSpeed() const { return m_breathSpeed; }
    float GetBreathIntensity() const { return m_breathIntensity; }

    // Getters untuk Window System
    Beyond::Window* GetMainWindow() const { return m_naviWindow; }
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

private:
    // Window & Camera
    Beyond::Window* m_naviWindow = nullptr;
    std::shared_ptr<Camera> m_naviCamera;

    // Transform Data (World Space)
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 5.0f };
    DirectX::XMFLOAT2 m_baseWindowSize = { 200.0f, 200.0f }; // <--- TAMBAHKAN INI (Ukuran Tetap)
    DirectX::XMFLOAT2 m_windowSize = { 400.0f, 400.0f };     // (Ukuran Dinamis untuk OS)

    float m_breathSpeed = 2.0f;
    float m_breathIntensity = 14.0f;

    // Visual
    std::unique_ptr<Sprite> m_faceSprite;
    float m_glitchTimer = 0.0f;
};