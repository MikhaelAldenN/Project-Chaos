#include "Item.h"
#include "System/Graphics.h"

using namespace DirectX;

Item::Item(ID3D11Device* device, const DirectX::XMFLOAT3& position, ItemType type)
{
    // Reuse Block Model for optimization
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");

    m_type = type;
    movement->SetPosition(position);
    movement->SetGravityEnabled(false);

    originalY = position.y;
    animTime = 0.0f;
}

void Item::Update(float elapsedTime, Camera* camera)
{
    if (!isActive) return;

    animTime += elapsedTime;

    // 1. Floating Animation (Sine Wave)
    XMFLOAT3 pos = movement->GetPosition();
    pos.y = originalY + sinf(animTime * FLOAT_SPEED) * FLOAT_AMP;
    movement->SetPosition(pos);

    // 2. Spinning Animation
    movement->SetRotation({ 0.0f, animTime * SPIN_SPEED, 0.0f });

    SyncData();
}

void Item::Render(ModelRenderer* renderer)
{
    if (!isActive) return;
    XMFLOAT3 currentScale = this->scale;
    Character::scale = currentScale;

    renderer->Draw(ShaderId::Phong, model, color);
}