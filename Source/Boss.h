#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "System/ModelRenderer.h"

// =========================================================
// BOSS PART - Single Component
// =========================================================
struct BossPart
{
    std::string name;  // Identifier untuk part ini
    std::shared_ptr<Model> model;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    // Hasil kalkulasi floating (untuk Tracking Window)
    DirectX::XMFLOAT3 visualPosition = { 0.0f, 0.0f, 0.0f };

    // Floating Settings
    bool useFloating = true;
    float timer = 0.0f;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };

    // Helper Functions
    void Update(float dt);
    void Render(ModelRenderer* renderer);
};

// =========================================================
// BOSS PART CONFIG - Data untuk Init
// =========================================================
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
// BOSS CLASS - Main Manager
// =========================================================
class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    void Render(ModelRenderer* renderer);
    void DrawDebugGUI();

    // --- PART MANAGEMENT ---
    // Tambah part baru secara dynamic
    bool AddPart(const BossPartConfig& config);

    // Get part by name
    BossPart* GetPart(const std::string& name);

    // Get visual position by name
    DirectX::XMFLOAT3 GetPartVisualPos(const std::string& name) const;

    // Check if part exists
    bool HasPart(const std::string& name) const;

    // Get all part names
    std::vector<std::string> GetPartNames() const;

    // --- LEGACY GETTERS (untuk backward compatibility) ---
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return GetPartVisualPos("monitor1"); }
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return GetPartVisualPos("cpu"); }
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return GetPartVisualPos("monitor2"); }
    DirectX::XMFLOAT3 GetMonitor3VisualPos() const { return GetPartVisualPos("monitor3"); }

    BossPart* GetMonitorPart() { return GetPart("monitor1"); }
    BossPart* GetCPUPart() { return GetPart("cpu"); }
    BossPart* GetMonitor2Part() { return GetPart("monitor2"); }
    BossPart* GetMonitor3Part() { return GetPart("monitor3"); }

private:
    void InitializeDefaultParts();

private:
    // Dynamic storage untuk semua parts
    std::vector<std::unique_ptr<BossPart>> m_parts;

    // Fast lookup by name
    std::unordered_map<std::string, BossPart*> m_partLookup;
};