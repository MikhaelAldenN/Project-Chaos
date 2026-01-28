#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

// Forward declarations untuk mengurangi dependency di header
class BitmapFont;
class Camera;

// =========================================================
// BOSS LOG ENTRY
// =========================================================
struct BossLogEntry {
    std::string text;
    int visibleChars = 0;
    float typeTimer = 0.0f;
    bool isDone = false;
};

// =========================================================
// BOSS TERMINAL CLASS
// Sistem console log bergaya 3D
// =========================================================
class BossTerminal
{
public:
    void Initialize(int maxLines);
    void Update(float dt);

    // Render butuh pointer ke Font dan Camera
    void Render(BitmapFont* font, Camera* camera,
        const DirectX::XMFLOAT3& parentPos,
        const DirectX::XMFLOAT3& parentRot);

    void AddLog(const std::string& message);

    // --- Configuration (Public for ImGui access) ---
    DirectX::XMFLOAT3 offset = { -2.3f, 3.8f, 0.0f }; // Offset dari Monitor Center
    DirectX::XMFLOAT3 rotationOffset = { 0.0f, 0.0f, 0.0f }; // Rotasi manual tambahan
    float scale = 0.011f;
    float lineSpacing = 1.0f; // Jarak antar baris dalam unit 3D
    DirectX::XMFLOAT4 color = { 0.93f, 0.75f, 0.06f, 1.0f }; // Amber CRT Color

private:
    std::vector<BossLogEntry> m_buffer;
    int m_maxLines = 10;
    int m_writeIndex = 0;
    int m_activeCount = 0;

    // Timer spawn log otomatis
    float m_spawnTimer = 0.0f;
    float m_nextSpawnDelay = 0.5f;

    float m_typeSpeed = 0.005f; // Kecepatan ketikan
};