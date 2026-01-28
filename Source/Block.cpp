#include "Block.h"
#include "System/Graphics.h"

using namespace DirectX;

Block::Block()
{
    auto device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");
    scale = defaultScale;

    // Initialize Sleep State
    isSleeping = false;
    sleepTimer = 0.0f;
}

Block::~Block()
{
}

void Block::Update(float elapsedTime, Camera* camera)
{
    if (!isActive) return;

    // If sleeping, do not sync data or move (Visual Optimization)
    // Note: The Manager will usually skip calling Update() entirely for sleeping blocks
    // but this is a safety check.
    if (isSleeping) return;

    SyncData();
}

void Block::Render(ModelRenderer* renderer, const DirectX::XMFLOAT4& color)
{
    if (!isActive) return;
    renderer->Draw(ShaderId::Phong, model, color);
}

void Block::OnHit()
{
    isActive = false;
}

void Block::TrySleep(float elapsedTime)
{
    // 1. Never sleep if we are programmed to move (Formation/Filling)
    if (isRelocating || isFilling || !isActive)
    {
        WakeUp();
        return;
    }

    if (!movement) return;

    // 2. Check Velocity Squared (Faster than Length)
    XMFLOAT3 vel = movement->GetVelocity();
    float vSq = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;

    if (vSq < SLEEP_VEL_SQ)
    {
        // Object is nearly stationary
        sleepTimer += elapsedTime;

        if (sleepTimer > SLEEP_TIME_REQ)
        {
            if (!isSleeping)
            {
                isSleeping = true;
                // Force absolute zero velocity to prevent micro-sliding
                movement->SetVelocity({ 0, 0, 0 });
            }
        }
    }
    else
    {
        // Object is moving fast
        WakeUp();
    }
}

void Block::WakeUp()
{
    isSleeping = false;
    sleepTimer = 0.0f;
}