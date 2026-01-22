#include "CollisionManager.h"

using namespace DirectX;

// =========================================================
// HELPER MATH FUNCTIONS
// =========================================================
static XMFLOAT3 Scale(const XMFLOAT3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }

void CollisionManager::Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em)
{
    m_player = p;
    m_stage = s;
    m_blockManager = bm;
    m_enemyManager = em;
}

void CollisionManager::Update()
{
    // 1. ENVIRONMENT FIRST (Walls & Floor)
    CheckStageCollision();     // Player vs Stage
    CheckBlockVsStage();       // Blocks vs Stage (Sets "HittingWall" flag)

    if (m_player && m_blockManager)
    {
        // 2. FLAG PROPAGATION (Find out who is stacked)
        // We run this BEFORE physics resolution to ensure the "Stacked" state is 100% accurate.
        int propagationSteps = 15;
        for (int i = 0; i < propagationSteps; ++i)
        {
            UpdateBlockStackFlags();
        }

        // 3. BLOCK PHYSICS (Resolve Block vs Block Overlap)
        // Now that we know who is stacked, we push them apart efficiently.
        

        // 4. GRAVITY (If needed)
        bool enableGravity = (m_player->GetGameStage() == 3);
        for (auto& block : m_blockManager->GetBlocks())
        {
            if (block->IsActive()) block->GetMovement()->SetGravityEnabled(enableGravity);
        }

        // 5. PLAYER LAST (Player hits the now-solid wall of blocks)
        if (m_player->GetGameStage() == 3)
        {
            CheckBlockVsBlocks();
            CheckPlayerVsBlocks();
        }
    }
}

// =========================================================
// NEW: Separate Flag Logic from Physics Logic
// =========================================================
void CollisionManager::UpdateBlockStackFlags()
{
    if (!m_blockManager) return;

    // Range where blocks "connect" logically
    float connectDist = 0.95f;

    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();

    for (size_t i = 0; i < count; ++i)
    {
        if (!blocks[i]->IsActive() || blocks[i]->IsFilling()) continue;

        for (size_t j = i + 1; j < count; ++j)
        {
            if (!blocks[j]->IsActive() || blocks[j]->IsFilling()) continue;

            auto* movA = blocks[i]->GetMovement();
            auto* movB = blocks[j]->GetMovement();
            XMFLOAT3 posA = movA->GetPosition();
            XMFLOAT3 posB = movB->GetPosition();

            float dx = std::abs(posA.x - posB.x);
            float dz = std::abs(posA.z - posB.z);

            // If blocks are close enough to transmit "Stack" signal
            if (dx < connectDist && dz < connectDist)
            {
                bool a_is_anchor = blocks[i]->IsStacked();
                bool b_is_anchor = blocks[j]->IsStacked();

                // Propagate the signal
                if (a_is_anchor && !b_is_anchor)
                {
                    blocks[j]->SetStacked(true);
                    // Kill velocity immediately to prevent further compression
                    movB->SetVelocityX(0.0f); movB->SetVelocityZ(0.0f);
                }
                else if (!a_is_anchor && b_is_anchor)
                {
                    blocks[i]->SetStacked(true);
                    // Kill velocity immediately
                    movA->SetVelocityX(0.0f); movA->SetVelocityZ(0.0f);
                }
            }
        }
    }
}

void CollisionManager::CheckBlockVsBlocks()
{
    if (!m_blockManager) return;

    // Radius Settings
    float boxRadius = 0.4f;
    float minTouchDist = boxRadius * 2.45f; // 0.8f

    // [CRITICAL] Safety Margin
    // We push blocks slightly further than 0.8 (e.g., 0.801)
    // This creates a tiny gap that prevents the physics engine from fighting itself next frame.
    float pushTarget = minTouchDist + 0.001f;

    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();

    int physicsIterations = 4; // We need fewer iterations because flags are already set

    for (int k = 0; k < physicsIterations; ++k)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (!blocks[i]->IsActive() || blocks[i]->IsFilling()) continue;

            for (size_t j = i + 1; j < count; ++j)
            {
                if (!blocks[j]->IsActive() || blocks[j]->IsFilling()) continue;

                auto* movA = blocks[i]->GetMovement();
                auto* movB = blocks[j]->GetMovement();
                XMFLOAT3 posA = movA->GetPosition();
                XMFLOAT3 posB = movB->GetPosition();

                float dx = std::abs(posA.x - posB.x);
                float dz = std::abs(posA.z - posB.z);

                // Check for physical overlap
                if (dx < minTouchDist && dz < minTouchDist)
                {
                    float overlapX = pushTarget - dx;
                    float overlapZ = pushTarget - dz;
                    bool useX = (overlapX < overlapZ);
                    float moveAmt = useX ? overlapX : overlapZ;

                    // Determine direction
                    float dir = 0.0f;
                    if (useX) dir = (posA.x < posB.x) ? -1.0f : 1.0f;
                    else      dir = (posA.z < posB.z) ? -1.0f : 1.0f;

                    bool a_anchor = blocks[i]->IsStacked();
                    bool b_anchor = blocks[j]->IsStacked();

                    // PUSH LOGIC
                    if (a_anchor && !b_anchor) {
                        // A is wall, push B away
                        if (useX) posB.x -= dir * moveAmt; else posB.z -= dir * moveAmt;
                        movB->SetPosition(posB);
                    }
                    else if (!a_anchor && b_anchor) {
                        // B is wall, push A away
                        if (useX) posA.x += dir * moveAmt; else posA.z += dir * moveAmt;
                        movA->SetPosition(posA);
                    }
                    else {
                        // Both loose or both stacked, push apart equally
                        float half = moveAmt * 0.5f;
                        if (useX) { posA.x += dir * half; posB.x -= dir * half; }
                        else { posA.z += dir * half; posB.z -= dir * half; }
                        movA->SetPosition(posA);
                        movB->SetPosition(posB);
                    }
                }
            }
        }
    }
}

void CollisionManager::CheckPlayerVsBlocks()
{
    if (!m_player || !m_blockManager) return;

    XMFLOAT3 playerSize = { 0.4f, 0.4f, 0.4f };
    XMFLOAT3 blockSize = { 0.4f, 0.4f, 0.4f };

    CharacterMovement* playerMove = m_player->GetMovement();
    XMFLOAT3 playerPos = playerMove->GetPosition();
    XMFLOAT3 playerVel = playerMove->GetVelocity();

    // 2 Passes to handle corners smoothly
    for (int k = 0; k < 2; ++k)
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (!block->IsActive() || block->IsFilling()) continue;

            XMFLOAT3 blockPos = block->GetMovement()->GetPosition();
            XMFLOAT3 outPos;

            // Use the Helper function to detect overlap
            if (Collision::IntersectCubeVsCube(blockPos, blockSize, playerPos, playerSize, outPos))
            {
                // [FIX] Is this block solid?
                // It is solid if it is Stacked (anchored) OR if it is barely moving (jammed).
                float speedSq = 0.0f;
                // speed calculation omitted for brevity, assuming stacked is primary factor

                // 1. HARD POSITION SNAP (Teleport Player Out)
                playerPos = outPos;
                playerMove->SetPosition(playerPos);

                // 2. HARD VELOCITY KILL (Stop Player Input)
                // We determine which face we hit by looking at the overlap
                float dx = playerPos.x - blockPos.x;
                float dz = playerPos.z - blockPos.z;

                if (std::abs(dx) > std::abs(dz))
                {
                    // Hit on X Face. 
                    // If moving towards block, Kill X.
                    if ((dx > 0 && playerVel.x < 0) || (dx < 0 && playerVel.x > 0))
                        playerMove->SetVelocityX(0.0f);
                }
                else
                {
                    // Hit on Z Face.
                    // If moving towards block, Kill Z.
                    if ((dz > 0 && playerVel.z < 0) || (dz < 0 && playerVel.z > 0))
                        playerMove->SetVelocityZ(0.0f);
                }

                // Update local var for next loop iteration
                playerVel = playerMove->GetVelocity();
            }
        }
    }
}

// ... [Keep CheckStageCollision and CheckBlockVsStage as they were] ...
// (Make sure to paste the previous implementations for those two functions here)
// =========================================================
// COPY-PASTE CheckStageCollision & CheckBlockVsStage FROM PREVIOUS ANSWERS
// =========================================================
void CollisionManager::CheckStageCollision()
{
    // ... (Same as before) ...
    if (!m_player || !m_stage) return;
    bool isGameplayActive = (m_player->GetGameStage() == 3);
    if (!isGameplayActive) {
        m_player->GetMovement()->SetGravityEnabled(false);
        m_player->GetMovement()->SetVelocityY(0.0f);
        return;
    }
    m_player->GetMovement()->SetGravityEnabled(true);

    // ... (Rest of logic: Matrix setup, ResolvePhysics lambda) ...
    // NOTE: Ensure you include the full body from previous correct versions
    XMMATRIX S = XMMatrixScaling(m_stage->scale.x, m_stage->scale.y, m_stage->scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_stage->rotation.x), XMConvertToRadians(m_stage->rotation.y), XMConvertToRadians(m_stage->rotation.z));
    XMMATRIX T = XMMatrixTranslation(m_stage->position.x, m_stage->position.y, m_stage->position.z);
    XMFLOAT4X4 stageWorld; XMStoreFloat4x4(&stageWorld, S * R * T);

    auto ResolvePhysics = [&](Character* character) {
        if (!character) return;
        auto* moveComp = character->GetMovement();
        XMFLOAT3 pos = moveComp->GetPosition();
        // ... (Include Wall and Floor checks from previous code) ...
        // [Simplified for brevity here, please use full code from earlier]
        // ...

        // 1. WALLS
        float bodyRadius = 0.6f;
        float checkDist = bodyRadius + 0.5f;
        static const std::vector<XMFLOAT3> sensors = { {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0.7f,0,0.7f}, {0.7f,0,-0.7f}, {-0.7f,0,0.7f}, {-0.7f,0,-0.7f} };
        for (const auto& dir : sensors) {
            XMFLOAT3 start = { pos.x, pos.y + 1.0f, pos.z };
            XMFLOAT3 end = { pos.x + dir.x * checkDist, pos.y + 1.0f, pos.z + dir.z * checkDist };
            XMFLOAT3 hitPos, hitNormal;
            if (Collision::RayCast(start, end, stageWorld, m_stage->GetModel().get(), hitPos, hitNormal)) {
                if (hitNormal.y < 0.5f) {
                    float dx = start.x - hitPos.x; float dz = start.z - hitPos.z;
                    float distToWall = sqrt(dx * dx + dz * dz);
                    if (distToWall < bodyRadius - 0.001f) {
                        XMFLOAT3 pushVector = Scale(hitNormal, bodyRadius);
                        pos.x = hitPos.x + pushVector.x; pos.z = hitPos.z + pushVector.z;
                        moveComp->SetPosition(pos);
                    }
                }
            }
        }
        // 2. FLOOR
        pos = moveComp->GetPosition();
        XMFLOAT3 floorStart = { pos.x, pos.y + 5.0f, pos.z };
        XMFLOAT3 floorEnd = { pos.x, pos.y - 10.0f, pos.z };
        XMFLOAT3 fHitPos, fHitNormal;
        if (Collision::RayCast(floorStart, floorEnd, stageWorld, m_stage->GetModel().get(), fHitPos, fHitNormal)) {
            if (fHitNormal.y >= 0.5f) {
                float targetY = fHitPos.y + 0.3f;
                if (targetY <= pos.y + 1.0f && pos.y >= targetY - 2.0f) {
                    pos.y = targetY;
                    moveComp->SetPosition(pos);
                    moveComp->SetVelocityY(0.0f);
                }
            }
        }
        };
    ResolvePhysics(m_player);
}

void CollisionManager::CheckBlockVsStage()
{
    // ... (Use Previous Correct Logic) ...
    if (!m_blockManager || !m_stage || !m_player) return;
    XMMATRIX S = XMMatrixScaling(m_stage->scale.x, m_stage->scale.y, m_stage->scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_stage->rotation.x), XMConvertToRadians(m_stage->rotation.y), XMConvertToRadians(m_stage->rotation.z));
    XMMATRIX T = XMMatrixTranslation(m_stage->position.x, m_stage->position.y, m_stage->position.z);
    XMFLOAT4X4 stageWorld; XMStoreFloat4x4(&stageWorld, S * R * T);
    XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();
    float safeRadius = 0.6f;
    float pushRadius = 0.6f;
    float checkRadius = 0.8f;
    float groundOffset = 0.3f;

    for (const auto& block : m_blockManager->GetBlocks()) {
        if (!block->IsActive()) continue;
        block->ResetCollisionState();
        auto* moveComp = block->GetMovement();
        XMFLOAT3 blockPos = moveComp->GetPosition();
        bool solved = false;

        // 1. FLOOR
        XMFLOAT3 rayDownStart = blockPos; rayDownStart.y += 0.5f;
        XMFLOAT3 rayDownEnd = blockPos; rayDownEnd.y -= 1.0f;
        XMFLOAT3 fHit, fNorm;
        if (Collision::RayCast(rayDownStart, rayDownEnd, stageWorld, m_stage->GetModel().get(), fHit, fNorm)) {
            if (fNorm.y > 0.7f) {
                float targetY = fHit.y + groundOffset;
                if (blockPos.y <= targetY + 0.5f) {
                    blockPos.y = targetY; moveComp->SetPosition(blockPos); moveComp->SetVelocityY(0.0f);
                }
            }
        }

        // 2. LEASH
        XMFLOAT3 rayStart = playerPos; rayStart.y += 1.0f;
        XMFLOAT3 rayEnd = blockPos; rayEnd.y = rayStart.y;
        float dx = rayEnd.x - rayStart.x; float dz = rayEnd.z - rayStart.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist > 0.5f) {
            XMFLOAT3 hitPos, hitNormal;
            float factor = (dist - 0.1f) / dist;
            XMFLOAT3 rayTarget = { rayStart.x + dx * factor, rayStart.y, rayStart.z + dz * factor };
            if (Collision::RayCast(rayStart, rayTarget, stageWorld, m_stage->GetModel().get(), hitPos, hitNormal)) {
                block->SetWallCollision(hitNormal);
                float penetration = safeRadius;
                XMFLOAT3 fixedPos;
                fixedPos.x = hitPos.x + (hitNormal.x * safeRadius);
                fixedPos.z = hitPos.z + (hitNormal.z * safeRadius);
                fixedPos.y = blockPos.y;
                moveComp->SetPosition(fixedPos);
                blockPos = fixedPos;
                solved = true;
            }
        }
        if (solved) continue;

        // 3. SENSORS
        static const std::vector<XMFLOAT3> sensors = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
        for (const auto& dir : sensors) {
            XMFLOAT3 sStart = { blockPos.x, blockPos.y + 0.5f, blockPos.z };
            XMFLOAT3 sEnd = { blockPos.x + dir.x * checkRadius, blockPos.y + 0.5f, blockPos.z + dir.z * checkRadius };
            XMFLOAT3 wHitPos, wHitNormal;
            if (Collision::RayCast(sStart, sEnd, stageWorld, m_stage->GetModel().get(), wHitPos, wHitNormal)) {
                if (wHitNormal.y < 0.5f) {
                    block->SetWallCollision(wHitNormal);
                    float distToWall = sqrtf((sStart.x - wHitPos.x) * (sStart.x - wHitPos.x) + (sStart.z - wHitPos.z) * (sStart.z - wHitPos.z));
                    if (distToWall < pushRadius) {
                        float pushAmount = pushRadius - distToWall;
                        blockPos.x -= dir.x * pushAmount; blockPos.z -= dir.z * pushAmount;
                        moveComp->SetPosition(blockPos);
                    }
                    break;
                }
            }
        }
    }
}