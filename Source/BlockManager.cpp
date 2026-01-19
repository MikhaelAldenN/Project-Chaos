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

    int center = 5;
    for (int r = -center; r <= center; ++r)
    {
        for (int c = -center; c <= center; ++c)
        {
            if (r == 0 && c == 0) continue;

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
    DirectX::XMFLOAT3 pVel = player->GetMovement()->GetVelocity();

    // Check Movement State
    bool isMoving = player->GetMovement()->IsMoving();

    if (isMoving)
    {
        m_formationTime += elapsedTime;
    }

    // --------------------------------------------------------
    // 1. SLOT ASSIGNMENT (Greedy / Cascading Fill)
    // --------------------------------------------------------
    if (m_formationBlocks.size() != m_sortedOffsets.size())
    {
        m_formationBlocks.assign(m_sortedOffsets.size(), nullptr);
    }

    std::vector<Block*> currentFrameSlots = m_formationBlocks;

    // Cleanup invalid pointers
    for (auto& slot : currentFrameSlots)
    {
        if (slot && !slot->IsActive()) slot = nullptr;
    }

    // Fill empty slots with closest available blocks
    for (int i = 0; i < currentFrameSlots.size(); ++i)
    {
        if (currentFrameSlots[i] != nullptr) continue;

        DirectX::XMFLOAT3 target;
        target.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        target.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);

        Block* bestCandidate = nullptr;
        int bestCandidateSourceIndex = -1;
        float bestDistSq = FLT_MAX;

        for (const auto& b : blocks)
        {
            if (!b->IsActive()) continue;

            // Check if block is already assigned to a higher priority slot
            int currentSlotIndex = -1;
            for (int k = 0; k < currentFrameSlots.size(); ++k)
            {
                if (currentFrameSlots[k] == b.get()) { currentSlotIndex = k; break; }
            }
            if (currentSlotIndex != -1 && currentSlotIndex <= i) continue;

            float d = GetDistSq(target, b->GetMovement()->GetPosition());
            if (d < bestDistSq)
            {
                bestDistSq = d;
                bestCandidate = b.get();
                bestCandidateSourceIndex = currentSlotIndex;
            }
        }

        if (bestCandidate != nullptr)
        {
            currentFrameSlots[i] = bestCandidate;
            if (bestCandidateSourceIndex != -1) currentFrameSlots[bestCandidateSourceIndex] = nullptr;
        }
    }
    m_formationBlocks = currentFrameSlots;

    // --------------------------------------------------------
    // 2. POSITION UPDATE (Movement & Jitter)
    // --------------------------------------------------------
    for (int i = 0; i < m_formationBlocks.size(); ++i)
    {
        Block* b = m_formationBlocks[i];
        if (!b) continue;

        // Calculate Base Target 
        DirectX::XMFLOAT3 target;
        target.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        target.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);
        target.y = 0.0f;

        // Apply Random Wiggle (If Moving) 
        if (isMoving)
        {
            float dx = target.x - playerPos.x;
            float dz = target.z - playerPos.z;
            float distToPlayerSq = dx * dx + dz * dz;

            if (distToPlayerSq > m_config.wiggleMinDistSq)
            {
                // Unique noise cycle for each block
                float noise = sinf(m_formationTime * m_config.noiseFrequency + (float)i * 13.1f);

                if (noise > m_config.noiseThreshold)
                {
                    float intensity = (noise - m_config.noiseThreshold) * m_config.intensityScale;

                    float wiggleX = sinf(m_formationTime * m_config.oscSpeedX + i * 7.0f);
                    float wiggleZ = cosf(m_formationTime * m_config.oscSpeedZ + i * 3.0f);

                    target.x += wiggleX * m_config.wiggleAmplitude * intensity;
                    target.z += wiggleZ * m_config.wiggleAmplitude * intensity;
                }
            }
        }

        // Calculate Smart Speed 
        DirectX::XMFLOAT3 currentPos = b->GetMovement()->GetPosition();

        // Dot product to check if block is "In Front" or "Behind" relative to movement
        float relativeDot = (m_sortedOffsets[i].x * pVel.x) + (m_sortedOffsets[i].z * pVel.z);

        float lerpSpeed = m_config.speedBase;

        if (relativeDot > 0.1f)      lerpSpeed = m_config.speedFront; // Snap fast (Front)
        else if (relativeDot < -0.1f) lerpSpeed = m_config.speedTrail; // Tight/Loose trail (Back)

        // Boost speed if lagging too far behind
        float distSq = GetDistSq(currentPos, target);
        if (distSq > m_config.catchUpThreshold)
        {
            lerpSpeed *= m_config.catchUpMult;
        }

        // Apply Movement 
        float finalT = std::clamp(lerpSpeed * elapsedTime, 0.0f, 1.0f);

        DirectX::XMFLOAT3 newPos;
        newPos.x = currentPos.x + (target.x - currentPos.x) * finalT;
        newPos.y = currentPos.y + (target.y - currentPos.y) * finalT;
        newPos.z = currentPos.z + (target.z - currentPos.z) * finalT;

        b->GetMovement()->SetPosition(newPos);
        b->SetRelocating(distSq > 0.01f);
    }
}

void BlockManager::Render(ModelRenderer* renderer)
{
    for (auto& block : blocks)
    {
        if (block->IsActive())
        {
            block->Render(renderer, globalBlockColor);
        }
    }
}

void BlockManager::CheckCollision(Ball* ball)
{
    if (!ball || !ball->IsActive()) return;

    DirectX::XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
    DirectX::XMFLOAT3 ballVel = ball->GetVelocity();
    DirectX::XMFLOAT3 prevBallPos = ball->GetPreviousPosition();

    float ballRadius = ball->GetRadius();
    float blockHalfSize = m_blockHalfSize;

    for (auto& block : blocks)
    {
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
            float maxZRange = blockHalfSize + ballRadius;
            float distPrevZ = std::abs(prevBallPos.z - blockPos.z);
            bool wasOutsideZ = (distPrevZ >= maxZRange);

            float maxXRange = blockHalfSize + ballRadius;
            float distPrevX = std::abs(prevBallPos.x - blockPos.x);
            bool wasOutsideX = (distPrevX >= maxXRange);

            if (wasOutsideZ)
            {
                ballVel.z *= -1.0f;
            }
            else if (wasOutsideX)
            {
                ballVel.x *= -1.0f;
            }
            else
            {
                if (overlapX < overlapZ) ballVel.x *= -1.0f;
                else ballVel.z *= -1.0f;
            }

            ball->SetVelocity(ballVel);
            block->OnHit();

            if (m_onBlockHitCallback)
            {
                m_onBlockHitCallback();
            }

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