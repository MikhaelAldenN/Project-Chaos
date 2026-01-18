#include "Stage.h"

using namespace DirectX;

Stage::Stage(ID3D11Device* device)
{
    // Load the model
    model = std::make_shared<Model>(device, StageConfig::MODEL_PATH);

    // Apply defaults
    position = StageConfig::DEFAULT_POS;
    rotation = StageConfig::DEFAULT_ROT;
    scale = StageConfig::DEFAULT_SCALE;
    color = StageConfig::DEFAULT_COLOR;

    UpdateTransform();
}

void Stage::UpdateTransform()
{
    if (!model) return;

    // Calculate World Matrix: Scale * Rotation * Translation
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );
    XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);

    XMMATRIX world = S * R * T;

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);

    // Apply to the model's root nodes
    model->UpdateTransform(worldFloat);
}

void Stage::Render(ModelRenderer* renderer)
{
    if (!model || !renderer) return;
    renderer->Draw(ShaderId::Phong, model, color);
}