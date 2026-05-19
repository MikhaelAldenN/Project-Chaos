#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "BeyondWindow.h"
#include "Camera.h"
#include "INaviPhase.h" // Interface yang kita buat di Tahap 1

class WindowTrackingSystem;
class Sprite;

struct FaceParams {
    float minInterval = 1.0f;
    float maxInterval = 2.0f;
    float chance2x2 = 30.0f;
    float faceTotalSize = 5.0f;
    bool  enableGlitch = true;

    // ==========================================
    // [NEW] EFEK FLICKER & WARNA NGACO
    // ==========================================
    float flickerChance = 1.5f;       // Peluang ubin berkedip instan per-frame (0% - 10%)
    float colorGlitchChance = 25.0f;  // Peluang RGB berubah redup/ngaco saat interval ganti (0% - 100%)
};

class NaviBoss {
public:
    NaviBoss();
    ~NaviBoss();

    // --- Lifecycle ---
    void Initialize(WindowTrackingSystem* windowSystem);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, Camera* currentCamera);

    // --- State Management ---
    void ChangePhase(std::unique_ptr<INaviPhase> newPhase);
    INaviPhase* GetCurrentPhase() const { return m_currentPhase.get(); } // <--- TAMBAHKAN BARIS INI

    // --- Core Getters (Untuk digunakan oleh Phase) ---
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }

    WindowTrackingSystem* GetWindowSystem() const { return m_windowSystem; }
    Beyond::Window* GetMainWindow() const { return m_naviWindow; }

    // Stats
    float GetHP() const { return m_hp; }
    void TakeDamage(float damage) { m_hp -= damage; }

    // --- Core Visuals (Breathing Head) ---
    void SetCoreBreathParams(float speed, float intensity) {
        m_breathSpeed = speed;
        m_breathIntensity = intensity;
    }
    // [FIX] TAMBAHKAN DUA BARIS INI:
    float GetCoreBreathSpeed() const { return m_breathSpeed; }
    float GetCoreBreathIntensity() const { return m_breathIntensity; }

    void SetBaseWindowSize(float w, float h) { m_baseWindowSize = { w, h }; }
    void SetWindowSize(float w, float h) { m_windowSize = { w, h }; }

    void InitializeFaceGrid(ID3D11Device* device);
    void UpdateFaceGlitch(float dt);
    void RenderFaceGrid(ID3D11DeviceContext* context, Camera* currentCamera);

    FaceParams& GetFaceParams() { return m_faceParams; }
private:
    // --- System Reference ---
    WindowTrackingSystem* m_windowSystem = nullptr;

    // --- State Machine ---
    std::unique_ptr<INaviPhase> m_currentPhase;

    // --- Physical Core (Abadi di semua fase) ---
    Beyond::Window* m_naviWindow = nullptr;
    std::shared_ptr<Camera> m_naviCamera;
    std::unique_ptr<Sprite> m_faceSprite;

    // --- Data Global ---
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_baseWindowSize = { 200.0f, 200.0f };
    DirectX::XMFLOAT2 m_windowSize = { 400.0f, 400.0f };

    float m_hp = 100.0f;
    float m_breathSpeed = 2.0f;
    float m_breathIntensity = 14.0f;
    float m_glitchTimer = 0.0f;
    float m_pixelToUnit = 40.0f;
    float m_breathTimer = 0.0f;

    void RandomizeFaceGrid();

    // ==========================================
        // DATA WAJAH (GLITCH MATRIX)
        // ==========================================
    static constexpr int FACE_GRID_SIZE = 8;
    std::vector<std::unique_ptr<Sprite>> m_faceTextures;

    // [NEW] Struct untuk menyimpan data individual setiap kotak
    struct FaceTile {
        int texIdx = 0;
        float timer = 0.0f;
        float interval = 0.1f; // Kecepatan berubahnya kotak ini
        int size = 1;          // 1 = 1x1 (Normal), 2 = 2x2 (Besar)
        DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    };

    FaceTile m_faceGrid[FACE_GRID_SIZE][FACE_GRID_SIZE];

    FaceParams m_faceParams;
    // (Hapus m_faceGlitchTimer dan m_faceGlitchInterval dari sini!)};
};