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
    animTime = ((float)rand() / RAND_MAX) * 10.0f; // Random offset
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

    // Override scale locally for rendering without affecting collider if needed, 
    // but here we sync scale to transform.
    XMFLOAT3 currentScale = this->scale;

    // Pass custom scale to model matrix calculation inside framework or manually:
    // Since Character::SyncData uses 'scale' member:
    Character::scale = currentScale;

    renderer->Draw(ShaderId::Phong, model, color);
}