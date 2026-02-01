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
    // Reserve space for optimization
    blocks.reserve((m_rows * m_columns) + 100);

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
            // Create unique_ptr directly
            auto newBlock = std::make_unique<Block>();

            float posX = startX + (x * m_xSpacing) + m_playerSpawnOffsetX;
            float posZ = startZ + (z * m_zSpacing) + m_zOffsetWorld;

            newBlock->GetMovement()->SetPosition({ posX, 0.0f, posZ });
            newBlock->GetMovement()->SetGravityEnabled(false);

            blocks.push_back(std::move(newBlock));
        }
    }
    CalculateShieldOffsets();
    InitPrioritySlots();
}

void BlockManager::AddBlockFromItem(const DirectX::XMFLOAT3& startPos)
{
    // 1. Find an inactive block to recycle
    Block* targetBlock = nullptr;
    for (auto& blockPtr : blocks)
    {
        if (!blockPtr->IsActive())
        {
            targetBlock = blockPtr.get();
            break;
        }
    }

    // 2. If no inactive block, expand vector
    if (!targetBlock)
    {
        blocks.push_back(std::make_unique<Block>());
        targetBlock = blocks.back().get();
    }

    // 3. Activate and Setup Animation
    targetBlock->SetActive(true);
    DirectX::XMFLOAT3 spawnPos = startPos;
    spawnPos.y = 0.0f;
    targetBlock->GetMovement()->SetPosition(spawnPos);
    targetBlock->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    targetBlock->GetMovement()->SetGravityEnabled(false);

    // Reset flags
    targetBlock->SetProjectile(false);
    targetBlock->SetFalling(false);

    // Set "Relocating" to true to suck it into the grid
    targetBlock->SetRelocating(true);
    targetBlock->WakeUp();
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

    // Sorting Logic
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
    if (m_isInvincible)
    {
        m_invincibleTimer -= elapsedTime;
        if (m_invincibleTimer <= 0.0f)
        {
            m_isInvincible = false;
            m_invincibleTimer = 0.0f;
        }
        else
        {
            float t = std::abs(std::sin(m_invincibleTimer * Player::InvincibleSettings::BlinkSpeed));

            DirectX::XMFLOAT4 colorGrey = Player::InvincibleSettings::GetColor();
            DirectX::XMFLOAT4 colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

            // Linear Interpolation
            m_currentInvincibleColor.x = colorGrey.x + (colorWhite.x - colorGrey.x) * t;
            m_currentInvincibleColor.y = colorGrey.y + (colorWhite.y - colorGrey.y) * t;
            m_currentInvincibleColor.z = colorGrey.z + (colorWhite.z - colorGrey.z) * t;
            m_currentInvincibleColor.w = 1.0f;
        }
    }

    if (m_shootTimer > 0.0f)
    {
        m_shootTimer -= elapsedTime;
    }

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
        for (auto& blockPtr : blocks) {
            if (blockPtr->IsActive() && !blockPtr->IsProjectile()) {
                blockPtr->GetMovement()->SetVelocityX(0);
                blockPtr->GetMovement()->SetVelocityZ(0);
            }
        }
    }

    float killPlaneY = Player::MovementSettings::KillPlaneY;

    // 2. Update Physics & Visuals
    for (auto& blockPtr : blocks)
    {
        // Dereference unique_ptr
        Block& block = *blockPtr;

        if (!block.IsActive()) continue;

        if (block.IsProjectile())
        {
            block.GetMovement()->Update(elapsedTime);
            block.Update(elapsedTime, camera);

            // Check Max Range (Distance from Player)
            if (player)
            {
                DirectX::XMFLOAT3 bPos = block.GetMovement()->GetPosition();
                DirectX::XMFLOAT3 pPos = player->GetMovement()->GetPosition();

                float dx = bPos.x - pPos.x;
                float dz = bPos.z - pPos.z;
                float distSq = dx * dx + dz * dz;

                // Check against MaxRange squared
                if (distSq > shootSettings.MaxRange * shootSettings.MaxRange)
                {
                    block.OnHit(); // Destroy block if too far
                }
            }

            // Kill Plane Check
            if (block.GetMovement()->GetPosition().y < killPlaneY) {
                block.OnHit();
            }
            continue;
        }

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
} // <--- INI YANG HILANG SEBELUMNYA!

void BlockManager::RestoreShield(int count, Player* player)
{
    ClearBlocks();
    isShieldActive = false;
    wasShieldActive = false;
    for (int i = 0; i < count; ++i)
    {
        SpawnAllyBlock(player);
    }
    ActivateFormationMode();
}

void BlockManager::SpawnAllyBlock(Player* player)
{
    if (!player) return;

    // 1. Buat Blok Biasa
    auto newBlock = std::make_unique<Block>();

    // 2. Set Posisi Awal
    DirectX::XMFLOAT3 pPos = player->GetMovement()->GetPosition();

    float offsetX = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;
    float offsetZ = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;

    newBlock->GetMovement()->SetPosition({ pPos.x + offsetX, 0.0f, pPos.z + offsetZ });

    // 3. MASUKKAN KE VECTOR UTAMA
    blocks.push_back(std::move(newBlock));
}

void BlockManager::UpdateShieldLogic(bool isInputHeld, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime)
{
    bool justActivated = (isInputHeld && shieldSettings.Enabled) && !wasShieldActive;
    isShieldActive = isInputHeld && shieldSettings.Enabled;
    wasShieldActive = isShieldActive;

    if (!isShieldActive) { m_shieldAssignments.clear(); return; }

    DirectX::XMFLOAT3 targetCenter = mouseWorldPos;
    float dx = targetCenter.x - playerPos.x;
    float dz = targetCenter.z - playerPos.z;
    float distSq = dx * dx + dz * dz;
    float maxDist = shieldSettings.MaxTetherDistance;

    if (distSq > maxDist * maxDist) {
        float dist = sqrtf(distSq);
        float scale = maxDist / dist;
        targetCenter.x = playerPos.x + (dx * scale);
        targetCenter.z = playerPos.z + (dz * scale);
        dx = targetCenter.x - playerPos.x;
        dz = targetCenter.z - playerPos.z;
    }
    targetCenter.y = 0.0f;
    float angle = atan2f(dx, dz) + DirectX::XM_PI;
    float cosA = cosf(angle); float sinA = sinf(angle);

    if (justActivated || m_shieldAssignments.empty()) {
        m_shieldAssignments.clear();
        std::vector<Block*> availableBlocks;

        // Loop over unique_ptrs
        for (auto& blockPtr : blocks) {
            if (blockPtr->IsActive() && !blockPtr->IsProjectile())
                availableBlocks.push_back(blockPtr.get());
        }

        for (int i = 0; i < m_shieldOffsets.size(); ++i) {
            if (availableBlocks.empty()) { m_shieldAssignments.push_back(nullptr); continue; }
            float ox = m_shieldOffsets[i].x; float oz = m_shieldOffsets[i].z;
            float rx = ox * cosA + oz * sinA; float rz = oz * cosA - ox * sinA;
            DirectX::XMFLOAT3 slotPos = { targetCenter.x + rx, 0.0f, targetCenter.z + rz };
            int bestIdx = -1; float bestDistSq = FLT_MAX;
            for (int b = 0; b < availableBlocks.size(); ++b) {
                DirectX::XMFLOAT3 bPos = availableBlocks[b]->GetMovement()->GetPosition();
                float dX = bPos.x - slotPos.x; float dZ = bPos.z - slotPos.z;
                float dSq = dX * dX + dZ * dZ;
                if (dSq < bestDistSq) { bestDistSq = dSq; bestIdx = b; }
            }
            if (bestIdx != -1) {
                m_shieldAssignments.push_back(availableBlocks[bestIdx]);
                availableBlocks[bestIdx] = availableBlocks.back(); availableBlocks.pop_back();
            }
            else { m_shieldAssignments.push_back(nullptr); }
        }
    }
    for (int i = 0; i < m_shieldAssignments.size(); ++i) {
        Block* b = m_shieldAssignments[i];
        if (!b || !b->IsActive()) continue;
        float ox = m_shieldOffsets[i].x; float oz = m_shieldOffsets[i].z;
        float rotX = ox * cosA + oz * sinA; float rotZ = oz * cosA - ox * sinA;
        DirectX::XMFLOAT3 finalTarget = { targetCenter.x + rotX, 0.0f, targetCenter.z + rotZ };
        DirectX::XMFLOAT3 currentPos = b->GetMovement()->GetPosition();
        float diffX = finalTarget.x - currentPos.x; float diffZ = finalTarget.z - currentPos.z;
        float distToTargetSq = diffX * diffX + diffZ * diffZ;
        bool isMoving = distToTargetSq > shieldSettings.ArrivalThresholdSq;
        b->SetRelocating(isMoving);
        float speed = shieldSettings.MoveSpeed;
        if (distToTargetSq > 5.0f) speed *= 2.0f;
        b->GetMovement()->SetVelocityX(diffX * speed);
        b->GetMovement()->SetVelocityZ(diffZ * speed);
        if (distToTargetSq > shieldSettings.WakeUpDistanceSq) b->WakeUp();
    }
}


void BlockManager::UpdateShootLogic(bool isInputPressed, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime)
{
    bool justPressed = isInputPressed && !m_wasShootPressed;
    m_wasShootPressed = isInputPressed;

    if (!justPressed) return;
    if (m_shootTimer > 0.0f) return;
    if (isShieldActive) return;

    Block* closestBlock = nullptr;
    float closestDistSq = FLT_MAX;
    int closestSlotIndex = -1;

    for (int i = 0; i < m_formationBlocks.size(); ++i)
    {
        Block* b = m_formationBlocks[i];
        if (!b || !b->IsActive() || b->IsProjectile()) continue;

        DirectX::XMFLOAT3 bPos = b->GetMovement()->GetPosition();
        float dx = bPos.x - mouseWorldPos.x;
        float dz = bPos.z - mouseWorldPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestBlock = b;
            closestSlotIndex = i;
        }
    }

    if (closestBlock)
    {
        DirectX::XMFLOAT3 startPos = closestBlock->GetMovement()->GetPosition();
        float dx = mouseWorldPos.x - startPos.x;
        float dz = mouseWorldPos.z - startPos.z;

        float length = std::sqrt(dx * dx + dz * dz);
        if (length > 0.001f)
        {
            dx /= length;
            dz /= length;

            float finalSpeed = shootSettings.ProjectileSpeed;
            finalSpeed += 5.0f; 

            if (IsInvincible())
            {
                auto newProjectile = std::make_unique<Block>();
                newProjectile->SetActive(true);
                newProjectile->GetMovement()->SetPosition(startPos);
                newProjectile->SetProjectile(true);
                newProjectile->GetMovement()->SetVelocityX(dx * finalSpeed);
                newProjectile->GetMovement()->SetVelocityZ(dz * finalSpeed);

                blocks.push_back(std::move(newProjectile));
            }
            else
            {
                closestBlock->SetProjectile(true);
                closestBlock->GetMovement()->SetVelocityX(dx * finalSpeed);
                closestBlock->GetMovement()->SetVelocityZ(dz * finalSpeed);

                if (closestSlotIndex != -1) {
                    m_formationBlocks[closestSlotIndex] = nullptr;
                }
            }

            m_shootTimer = shootSettings.Cooldown;
        }
    }
}

void BlockManager::UpdateFormationPositions(float elapsedTime, Player* player)
{
    if (!player) return;

    DirectX::XMFLOAT3 playerPos = player->GetMovement()->GetPosition();
    DirectX::XMFLOAT3 pVel = player->GetMovement()->GetVelocity();
    bool isMoving = player->GetMovement()->IsMoving();
    if (isMoving) m_formationTime += elapsedTime;

    if (m_formationBlocks.size() != m_sortedOffsets.size()) m_formationBlocks.assign(m_sortedOffsets.size(), nullptr);
    std::vector<Block*> currentFrameSlots = m_formationBlocks;

    for (auto& slot : currentFrameSlots) {
        if (slot && (!slot->IsActive() || slot->IsProjectile())) slot = nullptr;
    }

    for (int i = 0; i < currentFrameSlots.size(); ++i) {
        if (currentFrameSlots[i] != nullptr) continue;
        DirectX::XMFLOAT3 target;
        target.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        target.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);

        Block* bestCandidate = nullptr;
        int bestCandidateSourceIndex = -1;
        float bestDistSq = FLT_MAX;

        for (auto& bPtr : blocks) {
            // Use pointer access
            if (!bPtr->IsActive()) continue;
            if (bPtr->IsProjectile()) continue;

            int currentSlotIndex = -1;
            // Compare raw pointers
            for (int k = 0; k < currentFrameSlots.size(); ++k) {
                if (currentFrameSlots[k] == bPtr.get()) { currentSlotIndex = k; break; }
            }
            if (currentSlotIndex != -1 && currentSlotIndex <= i) continue;
            float d = GetDistSq(target, bPtr->GetMovement()->GetPosition());
            if (d < bestDistSq) { bestDistSq = d; bestCandidate = bPtr.get(); bestCandidateSourceIndex = currentSlotIndex; }
        }
        if (bestCandidate != nullptr) {
            currentFrameSlots[i] = bestCandidate;
            if (bestCandidateSourceIndex != -1) currentFrameSlots[bestCandidateSourceIndex] = nullptr;
        }
    }
    m_formationBlocks = currentFrameSlots;

    for (int i = 0; i < m_formationBlocks.size(); ++i) {
        Block* b = m_formationBlocks[i];
        if (!b) continue;
        if (b->IsFalling()) continue;

        DirectX::XMFLOAT3 currentPos = b->GetMovement()->GetPosition();

        DirectX::XMFLOAT3 target;
        target.x = playerPos.x + (m_sortedOffsets[i].x * formationSpacing);
        target.z = playerPos.z + (m_sortedOffsets[i].z * formationSpacing);
        target.y = 0.0f;

        if (isMoving) {
            float dx = target.x - playerPos.x;
            float dz = target.z - playerPos.z;
            float distToPlayerSq = dx * dx + dz * dz;
            if (distToPlayerSq > m_config.wiggleMinDistSq) {
                float noise = sinf(m_formationTime * m_config.noiseFrequency + (float)i * 13.1f);
                if (noise > m_config.noiseThreshold) {
                    float intensity = (noise - m_config.noiseThreshold) * m_config.intensityScale;
                    float wiggleX = sinf(m_formationTime * m_config.oscSpeedX + i * 7.0f);
                    float wiggleZ = cosf(m_formationTime * m_config.oscSpeedZ + i * 3.0f);
                    target.x += wiggleX * m_config.wiggleAmplitude * intensity;
                    target.z += wiggleZ * m_config.wiggleAmplitude * intensity;
                }
            }
        }

        float lerpSpeed = m_config.speedBase;
        float relativeDot = (m_sortedOffsets[i].x * pVel.x) + (m_sortedOffsets[i].z * pVel.z);
        if (relativeDot > 0.1f) lerpSpeed = m_config.speedFront;
        else if (relativeDot < -0.1f) lerpSpeed = m_config.speedTrail;

        float distSq = GetDistSq(currentPos, target);
        bool isLongDistance = (distSq > 0.1f);
        b->SetFilling(isLongDistance);

        if (distSq > m_config.catchUpThreshold) {
            lerpSpeed = m_config.speedBase * m_config.catchUpMult;
        }

        if (b->IsHittingWall()) {
            lerpSpeed = 5.0f;
        }

        float dx = target.x - currentPos.x;
        float dz = target.z - currentPos.z;
        float vx = dx * lerpSpeed;
        float vz = dz * lerpSpeed;

        if (b->IsHittingWall()) {
            float px = playerPos.x - currentPos.x;
            float pz = playerPos.z - currentPos.z;
            float pLen = sqrtf(px * px + pz * pz);

            if (pLen > 0.01f) {
                vx += (px / pLen) * 0.8f;
                vz += (pz / pLen) * 0.8f;
            }
        }

        if (b->IsHittingWall()) {
            DirectX::XMFLOAT3 normal = b->GetWallNormal();
            float dot = vx * normal.x + vz * normal.z;
            if (dot < 0.0f) {
                vx = vx - (normal.x * dot);
                vz = vz - (normal.z * dot);
            }
        }

        b->GetMovement()->SetVelocityX(vx);
        b->GetMovement()->SetVelocityZ(vz);
        b->SetRelocating(distSq > 0.01f);
    }
}

void BlockManager::Render(ModelRenderer* renderer)
{
    DirectX::XMFLOAT4 finalColor = m_isInvincible ? invincibleColor : globalBlockColor;

    if (m_isInvincible)
    {
        finalColor = m_currentInvincibleColor; 
    }
    else
    {
        finalColor = globalBlockColor; 
    }

    for (auto& blockPtr : blocks)
    {
        if (blockPtr->IsActive())
        {
            blockPtr->Render(renderer, finalColor);
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
    bool hitZ = false; bool hitX = false;
    float searchRangeSq = (maxRange * 2.0f) * (maxRange * 2.0f);

    for (auto& blockPtr : blocks) {
        if (!blockPtr->IsActive()) continue;
        if (isShieldActive && blockPtr->IsRelocating()) continue;
        if (blockPtr->IsProjectile()) continue;

        DirectX::XMFLOAT3 blockPos = blockPtr->GetMovement()->GetPosition();
        float dx = ballPos.x - blockPos.x; float dz = ballPos.z - blockPos.z;
        float distSq = dx * dx + dz * dz;
        if (distSq > searchRangeSq || distSq > closestDistSq) continue;
        float absX = std::abs(dx); float absZ = std::abs(dz);
        if (absX >= maxRange || absZ >= maxRange) continue;
        closestDistSq = distSq; closestBlock = blockPtr.get();

        DirectX::XMFLOAT3 prevBallPos = ball->GetPreviousPosition();
        float distPrevZ = std::abs(prevBallPos.z - blockPos.z);
        float distPrevX = std::abs(prevBallPos.x - blockPos.x);
        bool wasOutsideZ = (distPrevZ >= maxRange); bool wasOutsideX = (distPrevX >= maxRange);
        float overlapX = maxRange - absX; float overlapZ = maxRange - absZ;
        hitZ = false; hitX = false;
        if (wasOutsideZ) hitZ = true; else if (wasOutsideX) hitX = true;
        else { if (overlapX < overlapZ) hitX = true; else hitZ = true; }
    }

    if (closestBlock) {
        // 1. Handle Physics (Bounce) - This happens regardless of invincibility
        DirectX::XMFLOAT3 vel = ball->GetVelocity();
        DirectX::XMFLOAT3 pos = ball->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 bPos = closestBlock->GetMovement()->GetPosition();
        float pushBuffer = 0.001f;

        if (hitZ) {
            vel.z *= -1.0f;
            float dir = (pos.z > bPos.z) ? 1.0f : -1.0f;
            pos.z = bPos.z + (dir * (maxRange + pushBuffer));
        }
        else {
            vel.x *= -1.0f;
            float dir = (pos.x > bPos.x) ? 1.0f : -1.0f;
            pos.x = bPos.x + (dir * (maxRange + pushBuffer));
        }
        ball->SetVelocity(vel);
        ball->GetMovement()->SetPosition(pos);

        // 2. Handle Destruction - [MODIFIED] Check Invincibility
        if (!IsInvincible())
        {
            closestBlock->OnHit();
            if (m_onBlockHitCallback) m_onBlockHitCallback();
        }
    }
}

bool BlockManager::CheckEnemyCollision(Ball* ball)
{
    if (!ball || !ball->IsActive()) return false;

    DirectX::XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
    float ballRadius = ball->GetRadius();
    float blockHalfSize = m_blockHalfSize;
    float maxInteractionDist = blockHalfSize + ballRadius;
    float maxInteractionDistSq = maxInteractionDist * maxInteractionDist;

    for (auto& blockPtr : blocks) {
        if (!blockPtr->IsActive()) continue;
        if (blockPtr->IsProjectile()) continue;

        DirectX::XMFLOAT3 blockPos = blockPtr->GetMovement()->GetPosition();
        float deltaX = ballPos.x - blockPos.x;
        float deltaZ = ballPos.z - blockPos.z;
        float distSq = deltaX * deltaX + deltaZ * deltaZ;

        if (distSq > maxInteractionDistSq) continue;

        float absX = std::abs(deltaX);
        float absZ = std::abs(deltaZ);
        float overlapX = (blockHalfSize + ballRadius) - absX;
        float overlapZ = (blockHalfSize + ballRadius) - absZ;

        if (overlapX > 0 && overlapZ > 0)
        {
            if (!IsInvincible())
            {
                blockPtr->OnHit();
                if (m_onBlockHitCallback) m_onBlockHitCallback();
            }
            return true;
        }
    }
    return false;
}

int BlockManager::GetActiveBlockCount() const
{
    int count = 0;
    for (const auto& blockPtr : blocks)
    {
        if (blockPtr->IsActive())
        {
            count++;
        }
    }
    return count;
}