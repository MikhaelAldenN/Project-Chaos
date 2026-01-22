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

    // Setter untuk posisi (bisa diatur lewat GUI nanti)
    void SetPosition(float x, float y, float z) { position = { x, y, z }; }

private:
    std::shared_ptr<Model> model;

    // Transformasi
    DirectX::XMFLOAT3 position = { 0.0f, 10.0f, 10.0f }; // Default agak di atas dan belakang
    DirectX::XMFLOAT3 rotation = { 0.0f, 180.0f, 0.0f }; // Menghadap ke kamera (asumsi model menghadap Z+)
    DirectX::XMFLOAT3 scale = { 10.0f, 10.0f, 10.0f }; // Diperbesar biar terlihat Boss
};