#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include "System/ModelRenderer.h"

// [BARU] Struct untuk membungkus logika setiap bagian Boss (Monitor, CPU, dll)
struct BossPart
{
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

class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    void Render(ModelRenderer* renderer);
    void DrawDebugGUI();

    // --- GETTERS (Untuk Tracking Window) ---
    // Monitor (Kepala)
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return partMonitor.visualPosition; }
    BossPart& GetMonitorPart() { return partMonitor; }

    // CPU (Badan)
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return partCPU.visualPosition; }
    BossPart& GetCPUPart() { return partCPU; }

    // [BARU] Monitor 2 (Side/Extra)
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return partMonitor2.visualPosition; }
    BossPart& GetMonitor2Part() { return partMonitor2; }


private:
    // Kita memecah Boss menjadi komponen-komponen
    BossPart partMonitor;
    BossPart partCPU;
    BossPart partMonitor2;
};