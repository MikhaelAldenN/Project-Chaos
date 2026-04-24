#include "Character.h"

using namespace DirectX;

Character::Character()
{
    // Initialize standard movement component
    movement = new CharacterMovement();
}

Character::~Character()
{
    if (movement) delete movement;
}

void Character::Render(ModelRenderer* renderer)
{
    if (model) renderer->Draw(ShaderId::Lambert, model);
}

void Character::RenderDebug(const RenderContext& rc, ShapeRenderer* renderer)
{
    if (!movement) return;

    // Visualize collision capsule
    XMFLOAT3 pos = movement->GetPosition();
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT3 rot = movement->GetRotation();
    XMMATRIX R = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);

    XMFLOAT4X4 transform;
    XMStoreFloat4x4(&transform, R * T);

    renderer->DrawCapsule(transform, 0.5f, 1.6f, { 0, 1, 0, 1 });
}

void Character::SyncData()
{
    // Ensure model and nodes exist before accessing
    if (model && !model->GetNodes().empty())
    {
        Model::Node& rootNode = model->GetNodes().at(0);

        // 1. Sync Position
        rootNode.position = movement->GetPosition();

        // 2. Sync Rotation
        XMFLOAT3 rot = movement->GetRotation();
        DirectX::XMVECTOR qRot = DirectX::XMQuaternionRotationRollPitchYaw(rot.x, rot.y, rot.z);
        DirectX::XMStoreFloat4(&rootNode.rotation, qRot);

        // 3. Sync Scale
        rootNode.scale = scale;

        // 4. Update World Transform (Identity matrix as parent)
        DirectX::XMFLOAT4X4 identity;
        DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

        model->UpdateTransform(identity);
    }
}