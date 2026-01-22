#pragma once

#include <DirectXMath.h>
#include <memory>
#include "System/ModelRenderer.h"

class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    void Render(ModelRenderer* renderer);
    void DrawDebugGUI();

    // --- TRANSFORM SETTERS ---
    void SetPosition(float x, float y, float z) { position = { x, y, z }; }
    void SetRotation(float x, float y, float z) { rotation = { x, y, z }; }
    void SetScale(float x, float y, float z) { scale = { x, y, z }; }

    // --- FLOATING CONFIG SETTERS ---
    void SetFloatingConfig(bool enable, float speed, float intensity, DirectX::XMFLOAT3 axis)
    {
        useFloatingAnim = enable;
        floatSpeed = speed;
        floatIntensity = intensity;
        floatAxis = axis;
    }

    // --- GETTERS (Wajib untuk Tracking System) ---
    DirectX::XMFLOAT3 GetPosition() const { return position; } // Posisi Asli (Base)
    DirectX::XMFLOAT3 GetVisualPosition() const { return visualPosition; } // Posisi + Animasi

private:
    std::shared_ptr<Model> model;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    // Disimpan untuk getter visual (tracking window akan mengejar ini)
    DirectX::XMFLOAT3 visualPosition = { 0.0f, 0.0f, 0.0f };

    // --- ANIMATION CONFIG ---
    float timer = 0.0f;
    bool useFloatingAnim = true;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.5f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };
};