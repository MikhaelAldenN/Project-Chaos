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
    blocks.reserve(m_rows * m_columns);

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
                    float px = startX + (x * m_xSpacing) + m_playerSpawnOffsetX;
                    float pz = startZ + (z * m_zSpacing) + m_zOffsetWorld;

                    player->GetMovement()->SetPosition({ px, 0, pz });
                }
                continue;
            }

            // ---------------------------------------------------------
            // HANDLE BLOCKS
            // ---------------------------------------------------------
            blocks.emplace_back();
            Block& newBlock = blocks.back(); // Get reference to the new block

            float posX = startX + (x * m_xSpacing) + m_playerSpawnOffsetX;
            float posZ = startZ + (z * m_zSpacing) + m_zOffsetWorld;

            // Use dot (.) instead of arrow (->)
            newBlock.GetMovement()->SetPosition({ posX, 0.0f, posZ });
            newBlock.GetMovement()->SetGravityEnabled(false);

            // No push_back needed, it's already in there via emplace_back
        }
    }
    CalculateShieldOffsets();
    InitPrioritySlots();
}

void BlockManager::CalculateShieldOffsets()
{
    m_shieldOffsets.clear();

    float step = shieldSettings.Spacing;

    for (float z = 3.5f; z >= -3.5f; z -= 0.5f)
    {
        for (float x = -3.5f; x <= 3.5f; x += 0.5f)
        {
            float hx = x * 0.4f; // Width scale
            float hy = z * 0.4f; // Height scale

            float a = hx * hx + hy * hy - 1.0f;
            float result = (a * a * a) - (hx * hx * (hy * hy * hy));

            if (result <= 0.0f)
            {
                m_shieldOffsets.push_back({ x * step, 0.0f, (z + 0.5f) * step });
            }
        }
    }

    std::sort(m_shieldOffsets.begin(), m_shieldOffsets.end(),
        [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
            if (std::abs(a.z - b.z) > 0.1f) return a.z < b.z;
            return std::abs(a.x) < std::abs(b.x);
        });
}

void BlockManager::InitPrioritySlots()
{
    m_sortedOffsets.clear();

    int center = 6;
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

    // Sorting Logic: Square Shells (Symmetrical)
    std::sort(m_sortedOffsets.begin(), m_sortedOffsets.end(),
        [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
            float layerA = (std::max)(std::abs(a.x), std::abs(a.z));
            float layerB = (std::max)(std::abs(b.x), std::abs(b.z));

            if (layerA != layerB) return layerA < layerB;
            if (a.z != b.z) return a.z < b.z;
            return a.x < b.x;
        });
}

void BlockManager::Update(float elapsedTime, Camera* camera, Player* player)
{
    if (isShieldActive)
    {
        // Shield logic is handled in UpdateShieldLogic called from Scene
    }

    // 1. Calculate Desired Velocities
    else if (isFormationActive && player)
    {
        UpdateFormationPositions(elapsedTime, player);
    }
    else 
    {
        for (auto& block : blocks) {
            if (block.IsActive()) { 
                block.GetMovement()->SetVelocityX(0); 
                block.GetMovement()->SetVelocityZ(0); 
            }
        }
    }

    float killPlaneY = -30.0f;

    // 2. Update Physics & Visuals
    for (auto& block : blocks)
    {
        if (!block.IsActive()) continue;

        block.TrySleep(elapsedTime);

        if (block.IsSleeping()) 
        {
            block.Update(elapsedTime, camera);
            continue;
        }

        block.GetMovement()->Update(elapsedTime);

        // Kill plane check (inline)
        if (block.GetMovement()->GetPosition().y < killPlaneY) 
        {
            block.OnHit(); 
            continue;
        }
        block.Update(elapsedTime, camera);
    }
}

void BlockManager::UpdateShieldLogic(bool isInputHeld, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime)
{
    // 1. Toggle State
    bool justActivated = (isInputHeld && shieldSettings.Enabled) && !wasShieldActive;
    isShieldActive = isInputHeld && shieldSettings.Enabled;
    wasShieldActive = isShieldActive;

    if (!isShieldActive)
    {
        m_shieldAssignments.clear();
        return;
    }

    // 2. Tether Logic (Target Calculation)
    DirectX::XMFLOAT3 targetCenter = mouseWorldPos;
    float dx = targetCenter.x - playerPos.x;
    float dz = targetCenter.z - playerPos.z;
    float distSq = dx * dx + dz * dz;
    float maxDist = shieldSettings.MaxTetherDistance;

    if (distSq > maxDist * maxDist)
    {
        float dist = sqrtf(distSq);
        float scale = maxDist / dist;
        targetCenter.x = playerPos.x + (dx * scale);
        targetCenter.z = playerPos.z + (dz * scale);

        // Recalculate dx/dz for accurate rotation angle
        dx = targetCenter.x - playerPos.x;
        dz = targetCenter.z - playerPos.z;
    }
    targetCenter.y = 0.0f;

    // Calculate Angle (Player -> Mouse)
    float angle = atan2f(dx, dz) + DirectX::XM_PI;
    float cosA = cosf(angle);
    float sinA = sinf(angle);

    // 3. Assignment Phase (Runs Once)
    if (justActivated || m_shieldAssignments.empty())
    {
        m_shieldAssignments.clear();
        std::vector<Block*> availableBlocks;
        for (auto& block : blocks) {
            if (block.IsActive()) availableBlocks.push_back(&block);
        }

        for (int i = 0; i < m_shieldOffsets.size(); ++i)
        {
            if (availableBlocks.empty()) {
                m_shieldAssignments.push_back(nullptr);
                continue;
            }

            float ox = m_shieldOffsets[i].x;
            float oz = m_shieldOffsets[i].z;
            float rx = ox * cosA + oz * sinA;
            float rz = oz * cosA - ox * sinA;

            DirectX::XMFLOAT3 slotPos = {
                targetCenter.x + rx,
                0.0f,
                targetCenter.z + rz
            };

            int bestIdx = -1;
            float bestDistSq = FLT_MAX;

            for (int b = 0; b < availableBlocks.size(); ++b)
            {
                DirectX::XMFLOAT3 bPos = availableBlocks[b]->GetMovement()->GetPosition();
                float dX = bPos.x - slotPos.x;
                float dZ = bPos.z - slotPos.z;
                float dSq = dX * dX + dZ * dZ;

                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                    bestIdx = b;
                }
            }

            if (bestIdx != -1) {
                m_shieldAssignments.push_back(availableBlocks[bestIdx]);
                availableBlocks[bestIdx] = availableBlocks.back();
                availableBlocks.pop_back();
            }
            else {
                m_shieldAssignments.push_back(nullptr);
            }
        }
    }

    // 4. Movement Phase (Runs Every Frame)
    for (int i = 0; i < m_shieldAssignments.size(); ++i)
    {
        Block* b = m_shieldAssignments[i];
        if (!b || !b->IsActive()) continue;

        // Apply Rotation to Offset
        float ox = m_shieldOffsets[i].x;
        float oz = m_shieldOffsets[i].z;

        // Rotate offset
        float rotX = ox * cosA + oz * sinA;
        float rotZ = oz * cosA - ox * sinA;

        DirectX::XMFLOAT3 finalTarget = {
            targetCenter.x + rotX,
            0.0f,
            targetCenter.z + rotZ
        };

        // Move Block
        DirectX::XMFLOAT3 currentPos = b->GetMovement()->GetPosition();
        float diffX = finalTarget.x - currentPos.x;
        float diffZ = finalTarget.z - currentPos.z;
        float distToTargetSq = diffX * diffX + diffZ * diffZ;

        bool isMoving = distToTargetSq > shieldSettings.ArrivalThresholdSq;
        b->SetRelocating(isMoving);

        float speed = shieldSettings.MoveSpeed;
        if (distToTargetSq > 5.0f) speed *= 2.0f;

        b->GetMovement()->SetVelocityX(diffX * speed);
        b->GetMovement()->SetVelocityZ(diffZ * speed);

        if (distToTargetSq > shieldSettings.WakeUpDistanceSq) {
            b->WakeUp();
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

        for (auto& b : blocks)
        {
            if (!b.IsActive()) continue;

            int currentSlotIndex = -1;
            for (int k = 0; k < currentFrameSlots.size(); ++k)
            {
                if (currentFrameSlots[k] == &b) { currentSlotIndex = k; break; }
            }
            if (currentSlotIndex != -1 && currentSlotIndex <= i) continue;

            float d = GetDistSq(target, b.GetMovement()->GetPosition());

            if (d < bestDistSq)
            {
                bestDistSq = d;
                bestCandidate = &b;

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
    // 2. VELOCITY UPDATE (Replaces Position Teleport)
    // --------------------------------------------------------
    for (int i = 0; i < m_formationBlocks.size(); ++i)
    {
        Block* b = m_formationBlocks[i];
        if (!b) continue;
        if (b->IsFalling()) continue;

        // Calculate Base Target 
        DirectX::XMFLOAT3 target;
        target.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        target.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);
        target.y = b->GetMovement()->GetPosition().y;

        // Apply Random Wiggle (If Moving) 
        if (isMoving)
        {
            float dx = target.x - playerPos.x;
            float dz = target.z - playerPos.z;
            float distToPlayerSq = dx * dx + dz * dz;

            if (distToPlayerSq > m_config.wiggleMinDistSq)
            {
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
        float relativeDot = (m_sortedOffsets[i].x * pVel.x) + (m_sortedOffsets[i].z * pVel.z);
        float lerpSpeed = m_config.speedBase;

        if (relativeDot > 0.1f)      lerpSpeed = m_config.speedFront; 
        else if (relativeDot < -0.1f) lerpSpeed = m_config.speedTrail;

        float distSq = GetDistSq(currentPos, target);
        bool isLongDistance = (distSq > 0.1f);
        b->SetFilling(isLongDistance);

        if (distSq > m_config.catchUpThreshold)
        {
            lerpSpeed = m_config.speedBase * m_config.catchUpMult;
        }

        float dx = target.x - currentPos.x;
        float dz = target.z - currentPos.z;
        float vx = dx * lerpSpeed;
        float vz = dz * lerpSpeed;

        if (b->IsHittingWall())
        {
            DirectX::XMFLOAT3 normal = b->GetWallNormal();
            float dot = vx * normal.x + vz * normal.z;

            if (dot < 0.0f)
            {
                vx = vx - (normal.x * dot);
                vz = vz - (normal.z * dot);
            }
        }

        b->GetMovement()->SetVelocityX(vx);
        b->GetMovement()->SetVelocityZ(vz);
        
        b->SetRelocating(distSq > 0.01f);
    }
}

// ... (Rest of file: Render, CheckCollision, GetActiveBlockCount are unchanged) ...
void BlockManager::Render(ModelRenderer* renderer)
{
    for (auto& block : blocks)
    {
        if (block.IsActive())
        {
            block.Render(renderer, globalBlockColor);
        }
    }
}

void BlockManager::CheckCollision(Ball* ball)
{
    if (!ball || !ball->IsActive()) return;

    DirectX::XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
    float ballRadius = ball->GetRadius();
    float blockHalfSize = m_blockHalfSize;
    float maxRange = blockHalfSize + ballRadius;

    Block* closestBlock = nullptr;
    float closestDistSq = FLT_MAX;

    // Collision Flags
    bool hitZ = false;
    bool hitX = false;

    // [OPTIMIZATION] Pre-calculate boundary for fast reject
    float searchRangeSq = (maxRange * 2.0f) * (maxRange * 2.0f);

    for (auto& block : blocks)
    {
        if (!block.IsActive()) continue;
        if (isShieldActive && block.IsRelocating()) continue;

        DirectX::XMFLOAT3 blockPos = block.GetMovement()->GetPosition();

        // [OPTIMIZATION 1] Fast Squared Distance Check

        float dx = ballPos.x - blockPos.x;
        float dz = ballPos.z - blockPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > searchRangeSq || distSq > closestDistSq) continue;

        // [OPTIMIZATION 2] AABB Check (Detailed)
        float absX = std::abs(dx);
        float absZ = std::abs(dz);

        if (absX >= maxRange || absZ >= maxRange) continue;

        closestDistSq = distSq;
        closestBlock = &block;

        // Determine Bounce Direction immediately
        DirectX::XMFLOAT3 prevBallPos = ball->GetPreviousPosition();
        float distPrevZ = std::abs(prevBallPos.z - blockPos.z);
        float distPrevX = std::abs(prevBallPos.x - blockPos.x);

        bool wasOutsideZ = (distPrevZ >= maxRange);
        bool wasOutsideX = (distPrevX >= maxRange);

        float overlapX = maxRange - absX;
        float overlapZ = maxRange - absZ;

        hitZ = false;
        hitX = false;

        if (wasOutsideZ) hitZ = true;
        else if (wasOutsideX) hitX = true;
        else {
            if (overlapX < overlapZ) hitX = true;
            else hitZ = true;
        }
    }

    // Apply collision ONLY to the single closest block
    if (closestBlock)
    {
        DirectX::XMFLOAT3 vel = ball->GetVelocity();
        DirectX::XMFLOAT3 pos = ball->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 bPos = closestBlock->GetMovement()->GetPosition();

        float pushBuffer = 0.001f; 

        if (hitZ)
        {
            vel.z *= -1.0f;
            // Push along Z
            float dir = (pos.z > bPos.z) ? 1.0f : -1.0f;
            pos.z = bPos.z + (dir * (maxRange + pushBuffer));
        }
        else // hitX
        {
            vel.x *= -1.0f;
            // Push along X
            float dir = (pos.x > bPos.x) ? 1.0f : -1.0f;
            pos.x = bPos.x + (dir * (maxRange + pushBuffer));
        }

        ball->SetVelocity(vel);
        ball->GetMovement()->SetPosition(pos); 
        closestBlock->OnHit();

        if (m_onBlockHitCallback) m_onBlockHitCallback();
    }
}

bool BlockManager::CheckEnemyCollision(Ball* ball)
{
    if (!ball || !ball->IsActive()) return false;

    // ? FIX: Added DirectX:: prefix
    DirectX::XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
    float ballRadius = ball->GetRadius();
    float blockHalfSize = m_blockHalfSize;

    // ? PRE-CALCULATE: Max interaction distance
    float maxInteractionDist = blockHalfSize + ballRadius;
    float maxInteractionDistSq = maxInteractionDist * maxInteractionDist;

    for (auto& block : blocks)
    {
        if (!block.IsActive()) continue;

        // ? FIX: Added DirectX:: prefix
        DirectX::XMFLOAT3 blockPos = block.GetMovement()->GetPosition();

        // ? FAST REJECT: Distance check BEFORE detailed overlap test
        float deltaX = ballPos.x - blockPos.x;
        float deltaZ = ballPos.z - blockPos.z;
        float distSq = deltaX * deltaX + deltaZ * deltaZ;

        // If too far, skip (avoids abs() calculations)
        if (distSq > maxInteractionDistSq) continue;

        // Now do the detailed check
        float absX = std::abs(deltaX);
        float absZ = std::abs(deltaZ);

        float overlapX = (blockHalfSize + ballRadius) - absX;
        float overlapZ = (blockHalfSize + ballRadius) - absZ;

        if (overlapX > 0 && overlapZ > 0)
        {
            block.OnHit();
            return true;
        }
    }

    return false;
}

int BlockManager::GetActiveBlockCount() const
{
    int count = 0;
    for (const auto& block : blocks)
    {
        if (block.IsActive())
        {
            count++;
        }
    }
    return count;
}
