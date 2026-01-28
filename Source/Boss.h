#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "System/ModelRenderer.h"
#include "Camera.h" // Diperlukan untuk Render 3D Text

// Forward declaration
class BitmapFont;

// =========================================================
// BOSS PART - Single Component
// =========================================================
struct BossPart
{
    std::string name;
    std::shared_ptr<Model> model;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    DirectX::XMFLOAT3 visualPosition = { 0.0f, 0.0f, 0.0f };

    bool useFloating = true;
    float timer = 0.0f;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };

    void Update(float dt);
    void Render(ModelRenderer* renderer);
};

struct BossPartConfig
{
    std::string name;
    std::string modelPath;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    bool useFloating = true;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };
};

// =========================================================
// BOSS TERMINAL (3D LOG CONSOLE)
// =========================================================
struct BossLogEntry {
    std::string text;
    int visibleChars = 0;
    float typeTimer = 0.0f;
    bool isDone = false;
};

class BossTerminal
{
public:
    void Initialize(int maxLines);
    void Update(float dt);
    void Render(BitmapFont* font, Camera* camera,
        const DirectX::XMFLOAT3& parentPos,
        const DirectX::XMFLOAT3& parentRot);

    void AddLog(const std::string& message);

    // Config
    DirectX::XMFLOAT3 offset = { -2.3f, 3.8f, 0.0f }; // Offset dari Monitor Center
    float scale = 0.011f;
    float lineSpacing = 1.0f; // Jarak antar baris dalam unit 3D
    DirectX::XMFLOAT4 color = { 0.93f, 0.75f, 0.06f, 1.0f }; // Amber CRT Color

    DirectX::XMFLOAT3 rotationOffset = { 0.0f, 0.0f, 0.0f }; // [BARU] Tambahan rotasi manual

private:
    std::vector<BossLogEntry> m_buffer;
    int m_maxLines = 10;
    int m_writeIndex = 0;
    int m_activeCount = 0;

    // Timer spawn log otomatis
    float m_spawnTimer = 0.0f;
    float m_nextSpawnDelay = 0.5f;

    float m_typeSpeed = 0.005f; // Kecepatan ngetik
};

// =========================================================
// BOSS CLASS - Main Manager
// =========================================================
class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    // [UPDATE] Render sekarang butuh Camera pointer untuk teks 3D
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void DrawDebugGUI();

    bool AddPart(const BossPartConfig& config);
    BossPart* GetPart(const std::string& name);
    DirectX::XMFLOAT3 GetPartVisualPos(const std::string& name) const;
    bool HasPart(const std::string& name) const;
    std::vector<std::string> GetPartNames() const;

    // --- LEGACY GETTERS ---
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return GetPartVisualPos("monitor1"); }
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return GetPartVisualPos("cpu"); }
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return GetPartVisualPos("monitor2"); }
    DirectX::XMFLOAT3 GetMonitor3VisualPos() const { return GetPartVisualPos("monitor3"); }

    // [BARU] Public access untuk nambah log dari luar (misal saat player kena hit)
    void AddTerminalLog(const std::string& msg);

    BossPart* GetMonitorPart() { return GetPart("monitor1"); }
    BossPart* GetCPUPart() { return GetPart("cpu"); }
    BossPart* GetMonitor2Part() { return GetPart("monitor2"); }
    BossPart* GetMonitor3Part() { return GetPart("monitor3"); }
    BossPart* GetCable1() { return GetPart("cable1"); }

private:
    void InitializeDefaultParts();

private:
    std::vector<std::unique_ptr<BossPart>> m_parts;
    std::unordered_map<std::string, BossPart*> m_partLookup;

    // [BARU] Sistem Terminal 3D
    BossTerminal m_terminal;
};