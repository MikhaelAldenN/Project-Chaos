#include "BlockManager.h"

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
    InitPrioritySlots();
}

void BlockManager::InitPrioritySlots()
{
    m_sortedOffsets.clear();

    // Generate a generic grid around (0,0,0)
    int center = 5;
    for (int r = -center; r <= center; ++r)
    {
        for (int c = -center; c <= center; ++c)
        {
            if (r == 0 && c == 0) continue; // Skip center

            DirectX::XMFLOAT3 offset;
            offset.x = (float)c;
            offset.y = 0.0f;
            offset.z = (float)r;
            m_sortedOffsets.push_back(offset);
        }
    }

    // Sort: Closest offsets to center come first
    std::sort(m_sortedOffsets.begin(), m_sortedOffsets.end(),
        [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
            float distA = a.x * a.x + a.z * a.z;
            float distB = b.x * b.x + b.z * b.z;
            return distA < distB;
        });
}

void BlockManager::Update(float elapsedTime, Camera* camera, Player* player)
{
    if (isFormationActive && player)
    {
        UpdateFormationPositions(elapsedTime, player);
    }

    for (auto& block : blocks)
    {
        if (block->IsActive())
        {
            block->Update(elapsedTime, camera);
        }
    }
}

void BlockManager::UpdateFormationPositions(float elapsedTime, Player* player)
{
    if (!player) return;
    DirectX::XMFLOAT3 playerPos = player->GetMovement()->GetPosition();

    // 1. Identify active blocks
    std::vector<Block*> activeBlocks;
    for (const auto& b : blocks) {
        if (b->IsActive()) activeBlocks.push_back(b.get());
    }

    if (activeBlocks.empty()) return;

    // 2. Identify Target Positions
    int count = (int)activeBlocks.size();
    if (count > m_sortedOffsets.size()) count = (int)m_sortedOffsets.size();

    std::vector<DirectX::XMFLOAT3> targets;
    for (int i = 0; i < count; ++i)
    {
        DirectX::XMFLOAT3 t;
        t.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        t.y = 0.0f;
        t.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);
        targets.push_back(t);
    }

    // 3. Assign and Move Blocks
    std::vector<bool> blockAssigned(activeBlocks.size(), false);

    for (const auto& target : targets)
    {
        int bestBlockIdx = -1;
        float closestDistSq = FLT_MAX;

        // Find nearest block for this target
        for (int i = 0; i < activeBlocks.size(); ++i)
        {
            if (blockAssigned[i]) continue;

            float d = GetDistSq(target, activeBlocks[i]->GetMovement()->GetPosition());
            if (d < closestDistSq)
            {
                closestDistSq = d;
                bestBlockIdx = i;
            }
        }

        // Apply Movement
        if (bestBlockIdx != -1)
        {
            Block* b = activeBlocks[bestBlockIdx];
            blockAssigned[bestBlockIdx] = true;

            DirectX::XMFLOAT3 currentPos = b->GetMovement()->GetPosition();

            // --- NEW: Check Distance to determine Collision State ---
            float distSq = GetDistSq(currentPos, target);

            // If far away (moving), disable collision. If close (arrived), enable it.
            if (distSq > 0.05f)
            {
                b->SetRelocating(true); // Disable Collision
            }
            else
            {
                b->SetRelocating(false); // Enable Collision
            }

            // Lerp Logic
            float lerpSpeed = 5.0f * elapsedTime;
            DirectX::XMFLOAT3 newPos;
            newPos.x = currentPos.x + (target.x - currentPos.x) * lerpSpeed;
            newPos.y = currentPos.y + (target.y - currentPos.y) * lerpSpeed;
            newPos.z = currentPos.z + (target.z - currentPos.z) * lerpSpeed;

            b->GetMovement()->SetPosition(newPos);
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
        if (!block->IsActive() || block->IsRelocating()) continue;

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