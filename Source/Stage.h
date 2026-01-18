#pragma once

#include <DirectXMath.h>
#include <memory>
#include "System/ModelRenderer.h"

// ==========================================
// STAGE CONFIGURATION 
// ==========================================
namespace StageConfig
{
    static const char* MODEL_PATH = "Data/Model/Stage/PLACEHOLDER_mdl_Stage.glb";
    static const DirectX::XMFLOAT3 DEFAULT_POS      = { 0.0f, -0.3f, -30.0f }; 
    static const DirectX::XMFLOAT3 DEFAULT_ROT      = { 0.0f, 180.0f, 0.0f };
    static const DirectX::XMFLOAT3 DEFAULT_SCALE    = { 90.0f, 90.0f, 90.0f };
    static const DirectX::XMFLOAT4 DEFAULT_COLOR    = { 1.0f, 1.0f, 1.0f, 1.0f };
}

class Stage
{
public:
    Stage(ID3D11Device* device);
    ~Stage() = default;

    void UpdateTransform();
    void Render(ModelRenderer* renderer);

public:
    // Public variables for GUI editing
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation; 
    DirectX::XMFLOAT3 scale;
    DirectX::XMFLOAT4 color;

private:
    std::shared_ptr<Model> model;
};