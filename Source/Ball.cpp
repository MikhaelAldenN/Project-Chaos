#include "Ball.h"
#include "System/Graphics.h"
#include "System/Input.h" 

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
    velocity = { launchDirX, 0.0f, launchDirZ };
    XMVECTOR v = XMLoadFloat3(&velocity);
    v = XMVector3Normalize(v) * speed;
    XMStoreFloat3(&velocity, v);
}

void Ball::Update(float elapsedTime, Camera* camera)
{
    if (!isActive)
    {
        if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        {
            Launch();
        }
        SyncData();
        return; 
    }

    XMFLOAT3 pos = movement->GetPosition();
    pos.x += velocity.x * elapsedTime;
    pos.z += velocity.z * elapsedTime;

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

    // Bottom Boundary 
    if (pos.z < zLimitBottom)
    {
        Reset();
    }

    pos.y = 0.0f;

    movement->SetPosition(pos);
    SyncData();
}