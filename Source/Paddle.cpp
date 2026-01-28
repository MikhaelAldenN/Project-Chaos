#include "Paddle.h"
#include "System/Graphics.h"
#include <algorithm>
#include <cmath>
#include <Windows.h>
#include <limits>

using namespace DirectX;

Paddle::Paddle()
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Paddle.glb");
    movement->SetPosition({ startX, 0.0f, fixedZ });
}

Paddle::~Paddle()
{
}

void Paddle::Update(float elapsedTime, Camera* camera)
{
    if (!isActive) return;
    if (!isAIEnabled)
    {
        HandleInput();
    }

    movement->SetRotationY(0.0f);

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 vel = movement->GetVelocity();

    pos.x += vel.x * elapsedTime;

    // Clamp Position
    if (pos.x > xLimitRight) pos.x = xLimitRight;
    if (pos.x < xLimitLeft)  pos.x = xLimitLeft;

    pos.y = 0.0f;
    pos.z = fixedZ;

    movement->SetPosition(pos);
    SyncData();
}

void Paddle::UpdateAI(float elapsedTime, Ball* ball, BlockManager* blockManager)
{
    if (!isActive || !isAIEnabled || !ball) return;

    // LAUNCH LOGIC
    if (!ball->IsActive())
    {
        launchTimer += elapsedTime;
        if (launchTimer >= aiLaunchDelay)
        {
            ball->Launch();
            launchTimer = 0.0f;
        }
        return;
    }
    launchTimer = 0.0f;

    // DATA GATHERING
    XMFLOAT3 ballPos = ball->GetPosition();
    XMFLOAT3 ballVel = ball->GetVelocity();
    XMFLOAT3 myPos = movement->GetPosition();

    float targetX = myPos.x;

    // Is ball coming towards us?
    bool isIncoming = (ballVel.z < 0.0f) && (ballPos.z > fixedZ);

    if (isIncoming)
    {
        // Active Mode: Sniper Targeting
        float distZ = ballPos.z - fixedZ;
        float timeToImpact = (ballVel.z != 0.0f) ? (distZ / fabsf(ballVel.z)) : 0.0f;
        float predictedBallX = ballPos.x + (ballVel.x * timeToImpact);

        // Wall Bounce Prediction
        int safety = 0;
        while ((predictedBallX < xLimitLeft || predictedBallX > xLimitRight) && safety < aiPredictionSteps)
        {
            if (predictedBallX < xLimitLeft)
                predictedBallX = xLimitLeft + (xLimitLeft - predictedBallX);
            else if (predictedBallX > xLimitRight)
                predictedBallX = xLimitRight - (predictedBallX - xLimitRight);

            safety++;
        }

        // Find Closest Block
        XMFLOAT3 targetBlockPos = { 0, 0, 10.0f };
        bool foundTarget = false;

        if (blockManager)
        {
            float closestDist = (std::numeric_limits<float>::max)();

            // [PERBAIKAN DI SINI] Menggunakan const auto& block (yang isinya unique_ptr)
            for (const auto& block : blockManager->GetBlocks())
            {
                // [FIX] Gunakan tanda panah (->) untuk mengakses unique_ptr
                if (!block->IsActive()) continue;

                XMFLOAT3 bPos = block->GetMovement()->GetPosition();
                float dx = bPos.x - myPos.x;
                float dz = bPos.z - myPos.z;
                float dist = (dx * dx) + (dz * dz);

                if (dist < closestDist)
                {
                    closestDist = dist;
                    targetBlockPos = bPos;
                    foundTarget = true;
                }
            }
        }

        // Aim Calculation
        float aimOffset = 0.0f;
        if (foundTarget)
        {
            float dx = targetBlockPos.x - predictedBallX;
            float dz = targetBlockPos.z - fixedZ;
            float desiredAngle = atan2f(dx, dz);
            float relativeIntersect = desiredAngle / maxBounceAngle;

            if (relativeIntersect > 1.0f) relativeIntersect = 1.0f;
            if (relativeIntersect < -1.0f) relativeIntersect = -1.0f;

            aimOffset = relativeIntersect * paddleHalfWidth;
        }
        targetX = predictedBallX - aimOffset;
    }
    else
    {
        // Idle Mode
        static float idleTimer = 0.0f;
        static float idleTargetX = 0.0f;

        idleTimer -= elapsedTime;
        if (idleTimer <= 0.0f)
        {
            idleTargetX = GetRandomFloat(aiIdlePatrolRange);
            idleTimer = aiIdleBaseWait + ((float)(rand() % 100) / 100.0f * aiIdleRandomWait);
        }
        targetX = idleTargetX;
    }

    // APPLY MOVEMENT
    if (targetX < xLimitLeft) targetX = xLimitLeft;
    if (targetX > xLimitRight) targetX = xLimitRight;

    float diffX = targetX - myPos.x;
    float velocityX = 0.0f;

    if (fabs(diffX) > aiMoveTolerance)
    {
        if (diffX > 0) velocityX = paddleSpeed;
        else           velocityX = -paddleSpeed;
    }

    movement->SetMoveInput(velocityX, 0.0f);
}

void Paddle::HandleInput()
{
    if (isAIEnabled) return;

    float xInput = 0.0f;
    if (GetAsyncKeyState('D') & 0x8000) xInput += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) xInput -= 1.0f;

    float velocityX = xInput * paddleSpeed;
    movement->SetMoveInput(velocityX, 0.0f);
}

void Paddle::CheckCollision(Ball* ball)
{
    if (!isActive || !ball) return;

    auto ballPos = ball->GetPosition();
    auto padPos = movement->GetPosition();

    float ballRadius = ball->GetRadius();
    float zDist = fabs(ballPos.z - padPos.z);

    // Check Depth Collision
    if (zDist < (paddleHalfDepth + ballRadius))
    {
        float xDist = fabs(ballPos.x - padPos.x);

        // Check Width Collision
        if (xDist < (paddleHalfWidth + ballRadius))
        {
            XMVECTOR vVel = XMLoadFloat3(&ball->GetVelocity());
            float speed = XMVectorGetX(XMVector3Length(vVel));

            // Calculate bounce angle based on where it hit the paddle
            float relativeIntersectX = (ballPos.x - padPos.x) / paddleHalfWidth;
            float bounceAngle = relativeIntersectX * maxBounceAngle;

            float newVx = speed * sinf(bounceAngle);
            float newVz = speed * cosf(bounceAngle);

            newVz = fabsf(newVz);

            ball->SetVelocity({ newVx, 0.0f, newVz });
        }
    }
}