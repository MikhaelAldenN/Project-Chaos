#include "Bullet.h"

using namespace DirectX;

Bullet::Bullet()
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    isActive = false;
    velocity = { 0, 0, 0 };
    movement->SetRotationY(0.0f);
    SyncData();
}

void Bullet::Fire(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& direction, float projectileSpeed)
{
    isActive = true;
    movement->SetPosition(startPos);

    XMVECTOR vDir = XMLoadFloat3(&direction);
    vDir = XMVector3Normalize(vDir);
    XMVECTOR vVel = vDir * projectileSpeed;
    XMStoreFloat3(&velocity, vVel);

    SyncData();
}

void Bullet::Update(float elapsedTime, Camera* camera)
{
    if (!isActive) return;

    if (m_homingTarget)
    {
        DirectX::XMFLOAT3 myPos = movement->GetPosition();
        DirectX::XMFLOAT3 ePos = m_homingTarget->GetPosition();
        DirectX::XMFLOAT3 eVel = m_homingTarget->GetMovement()->GetVelocity();

        float dx = ePos.x - myPos.x;
        float dz = ePos.z - myPos.z;
        float distance = std::sqrt((dx * dx) + (dz * dz));

        DirectX::XMVECTOR vCurrentVel = DirectX::XMLoadFloat3(&velocity);
        float currentSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(vCurrentVel));

        float leadTime = distance / currentSpeed;

        DirectX::XMFLOAT3 predictedPos = {
            ePos.x + (eVel.x * leadTime),
            ePos.y,
            ePos.z + (eVel.z * leadTime)
        };

        DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&myPos);
        DirectX::XMVECTOR vTarget = DirectX::XMLoadFloat3(&predictedPos);
        DirectX::XMVECTOR vDirToTarget = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(vTarget, vPos));

        float dynamicTurnSpeed = m_turnSpeed;
        if (distance < 5.0f) {
            dynamicTurnSpeed *= (5.0f / max(distance, 0.1f));
        }

        DirectX::XMVECTOR vDesiredVel = DirectX::XMVectorScale(vDirToTarget, currentSpeed);
        vCurrentVel = DirectX::XMVectorLerp(vCurrentVel, vDesiredVel, dynamicTurnSpeed * elapsedTime);

        vCurrentVel = DirectX::XMVectorScale(DirectX::XMVector3Normalize(vCurrentVel), currentSpeed);
        DirectX::XMStoreFloat3(&velocity, vCurrentVel);
    }

    XMFLOAT3 pos = movement->GetPosition();

    // Apply velocity
    pos.x += velocity.x * elapsedTime;
    pos.z += velocity.z * elapsedTime;

    movement->SetPosition(pos);
    SyncData();
}

void Bullet::ApplyMovement(const DirectX::XMFLOAT3& newPos, const DirectX::XMFLOAT3& newVel)
{
    movement->SetPosition(newPos);
    velocity = newVel;
    SyncData();
}