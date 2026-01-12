#include "BlockManager.h"
#include <cmath>

BlockManager::BlockManager()
{
}

BlockManager::~BlockManager()
{
    blocks.clear();
}

void BlockManager::Initialize(Player* player)
{
    blocks.clear();

    float startX = -((m_columns - 1) * m_xSpacing) / 2.0f;
    float startZ = -((m_rows - 1) * m_zSpacing) / 2.0f;

    int centerCol = m_columns / 2;
    int centerRow = m_rows / 2;

    for (int z = 0; z < m_rows; ++z)
    {
        for (int x = 0; x < m_columns; ++x)
        {
            // ---------------------------------------------------------
            // HANDLE PLAYER (The Hole)
            // ---------------------------------------------------------
            if (x == centerCol && z == centerRow)
            {
                if (player)
                {
                    // Apply offset to Player
                    float px = startX + (x * m_xSpacing) + m_playerSpawnOffsetX;
                    float pz = startZ + (z * m_zSpacing) + m_zOffsetWorld;

                    player->GetMovement()->SetPosition({ px, 0, pz });
                }
                continue; 
            }

            // ---------------------------------------------------------
            // HANDLE BLOCKS
            // ---------------------------------------------------------
            auto newBlock = std::make_unique<Block>();

            // Apply offset to Blocks too (so they move with player)
            float posX = startX + (x * m_xSpacing) + m_playerSpawnOffsetX;
            float posZ = startZ + (z * m_zSpacing) + m_zOffsetWorld;
            newBlock->GetMovement()->SetPosition({ posX, 0.0f, posZ });
            blocks.push_back(std::move(newBlock));
        }
    }
}

void BlockManager::Update(float elapsedTime, Camera* camera)
{
    for (auto& block : blocks)
    {
        if (block->IsActive())
        {
            block->Update(elapsedTime, camera);
        }
    }
}

void BlockManager::Render(ModelRenderer* renderer)
{
    for (auto& block : blocks)
    {
        if (block->IsActive())
        {
            block->Render(renderer);
        }
    }
}

void BlockManager::CheckCollision(Ball* ball)
{
    if (!ball || !ball->IsActive()) return;

    DirectX::XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
    DirectX::XMFLOAT3 ballVel = ball->GetVelocity();

    float ballRadius = ball->GetRadius();
    float blockHalfSize = m_blockHalfSize;

    for (auto& block : blocks)
    {
        // Skip dead blocks
        if (!block->IsActive()) continue;

        DirectX::XMFLOAT3 blockPos = block->GetMovement()->GetPosition();

        float deltaX = ballPos.x - blockPos.x;
        float deltaZ = ballPos.z - blockPos.z;

        float absX = std::abs(deltaX);
        float absZ = std::abs(deltaZ);

        float overlapX = (blockHalfSize + ballRadius) - absX;
        float overlapZ = (blockHalfSize + ballRadius) - absZ;

        if (overlapX > 0 && overlapZ > 0)
        {
            if (overlapX < overlapZ)
            {
                ballVel.x *= -1.0f;
            }
            else
            {
                ballVel.z *= -1.0f;
            }

            ball->SetVelocity(ballVel);
            block->OnHit();

            return;
        }
    }
}

int BlockManager::GetActiveBlockCount() const
{
    int count = 0;
    for (const auto& block : blocks)
    {
        if (block->IsActive())
        {
            count++;
        }
    }
    return count;
}