#include "Ball.h"
#include "System/Graphics.h"
#include "System/Input.h" 
#include <cmath>

using namespace DirectX;

Ball::Ball()
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    Reset();
}

Ball::~Ball()
{
}

void Ball::Reset()
{
    isActive = false;
    velocity = { 0, 0, 0 };
    movement->SetRotationY(0.0f);
    SyncData();
}

void Ball::Launch()
{
    if (isActive) return;
    isActive = true;

    float dirX = (rand() % 2 == 0) ? 1.0f : -1.0f;
    float range = launchMaxX - launchMinX;
    float randomPct = (float)(rand() % 100) / 100.0f; 
    float strength = launchMinX + (range * randomPct);
    velocity = { dirX * strength, 0.0f, 1.0f };
    XMVECTOR v = XMLoadFloat3(&velocity);
    v = XMVector3Normalize(v) * speed;
    XMStoreFloat3(&velocity, v);
}

void Ball::Fire(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& direction, float projectileSpeed)
{
    isActive = true;
    boundariesEnabled = false;
    movement->SetPosition(startPos);
    XMVECTOR vDir = XMLoadFloat3(&direction);
    vDir = XMVector3Normalize(vDir);
    XMVECTOR vVel = vDir * projectileSpeed;
    XMStoreFloat3(&velocity, vVel);
    SyncData();
}

void Ball::Update(float elapsedTime, Camera* camera)
{
    if (!isActive)
    {
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) Launch();
        SyncData();
        return;
    }

    XMFLOAT3 pos = movement->GetPosition();
    prevPosition = pos;
    pos.x += velocity.x * elapsedTime;
    pos.z += velocity.z * elapsedTime;
    float minAbsZ = speed * 0.25f;

    if (boundariesEnabled)
    {
        if (std::abs(velocity.z) < minAbsZ)
        {
            float dirZ = (velocity.z >= 0.0f) ? 1.0f : -1.0f;
            velocity.z = dirZ * minAbsZ;
            XMVECTOR v = XMLoadFloat3(&velocity);
            v = XMVector3Normalize(v) * speed;
            XMStoreFloat3(&velocity, v);
        }

        // Right Wall Bounce
        if (pos.x > xLimitRight)
        {
            pos.x = xLimitRight;
            velocity.x *= -1;
        }
        // Left Wall Bounce
        else if (pos.x < xLimitLeft)
        {
            pos.x = xLimitLeft;
            velocity.x *= -1;
        }
        // Top Wall
        if (pos.z > zLimitTop)
        {
            pos.z = zLimitTop;
            velocity.z *= -1;
        }
        // Bottom Boundary (Reset)
        if (pos.z < zLimitBottom)
        {
            Reset();
        }
    }

    pos.y = 0.0f;
    movement->SetPosition(pos);
    SyncData();
}