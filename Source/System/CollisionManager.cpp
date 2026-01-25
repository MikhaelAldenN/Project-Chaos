#include "CollisionManager.h"

using namespace DirectX;

// =========================================================
// ? FIXED: Transform World Position to Local Space
// =========================================================
static XMVECTOR TransformToLocal(const XMFLOAT3& worldPos, const DebugWallData& wall)
{
    XMVECTOR vWorldPos = XMLoadFloat3(&worldPos);
    XMVECTOR vWallPos = XMLoadFloat3(&wall.Position);

    // Step 1: Translate to wall's origin
    XMVECTOR vRelative = XMVectorSubtract(vWorldPos, vWallPos);

    // Step 2: Apply inverse rotation
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );

    // Inverse rotation = transpose for orthogonal matrices
    XMMATRIX matInvRot = XMMatrixTranspose(matRot);

    // Transform the relative position by inverse rotation
    return XMVector3TransformNormal(vRelative, matInvRot);
}

// =========================================================
// ? FIXED: Transform Local Direction to World Space
// =========================================================
static XMVECTOR TransformToWorld(const XMVECTOR& localVec, const DebugWallData& wall)
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );

    // For directions/vectors, TransformNormal is correct (no translation)
    return XMVector3TransformNormal(localVec, matRot);
}

// =========================================================
// ? FIXED: Transform Local Normal to World Space
// =========================================================
static XMVECTOR TransformNormalToWorld(const XMVECTOR& localNorm, const DebugWallData& wall)
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );

    // Normals need special handling for non-uniform scales
    // For now, we assume uniform scale, so just rotate
    return XMVector3TransformNormal(localNorm, matRot);
}

static XMVECTOR TransformToLocalLine(const XMFLOAT3& worldPos, const DebugLineData& line)
{
    XMVECTOR vWorldPos = XMLoadFloat3(&worldPos);
    XMVECTOR vLinePos = XMLoadFloat3(&line.Position);
    XMVECTOR vRelPos = XMVectorSubtract(vWorldPos, vLinePos);
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(line.Rotation.x),
        XMConvertToRadians(line.Rotation.y),
        XMConvertToRadians(line.Rotation.z)
    );
    XMMATRIX matInvRot = XMMatrixTranspose(matRot);
    return XMVector3TransformNormal(vRelPos, matInvRot);
}

void CollisionManager::Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em)
{
    m_player = p;
    m_stage = s;
    m_blockManager = bm;
    m_enemyManager = em;
}

void CollisionManager::Update(float elapsedTime)
{
    if (!m_player) return;

    // 1. FORCE DISABLE GRAVITY
    m_player->GetMovement()->SetGravityEnabled(false);
    m_player->GetMovement()->SetVelocityY(0.0f);

    if (m_blockManager)
    {
        // ? FIX: Use auto& (reference) and Dot (.) syntax
        for (auto& block : m_blockManager->GetBlocks())
        {
            if (block.IsActive()) // ? Changed -> to .
            {
                block.GetMovement()->SetGravityEnabled(false); // ? Changed -> to .
                block.GetMovement()->SetVelocityY(0.0f);       // ? Changed -> to .
            }
        }
    }

    if (m_player->GetGameStage() != 3) return;

    CheckEnemyProjectilesFull(elapsedTime);
    CheckStageCollision();
    CheckBlockVsStage();

    if (m_blockManager)
    {
        int propagationSteps = 15;
        for (int i = 0; i < propagationSteps; ++i)
        {
            UpdateBlockStackFlags();
        }
        CheckBlockVsBlocks();
        CheckPlayerVsBlocks();
    }
}

// =========================================================
// ? PRODUCTION-GRADE: Robust Edge Collision Detection
// =========================================================
void CollisionManager::CheckEnemyProjectilesFull(float elapsedTime)
{
    if (!m_enemyManager) return;

    // ? CRITICAL: Use larger epsilon for gameplay-scale objects
    static const float PARALLEL_EPSILON = 1e-4f;  // Was 1e-6f
    static const float SKIN_WIDTH = 0.01f;        // Safety margin
    static const float MIN_HIT_TIME = -0.001f;    // Allow slight back-penetration

    for (auto& enemy : m_enemyManager->GetEnemies())
    {
        auto& projectiles = enemy->GetProjectiles();
        AttackType type = enemy->GetAttackType();

        for (auto it = projectiles.begin(); it != projectiles.end(); )
        {
            auto& ball = *it;
            if (!ball) {
                ++it;
                continue;
            }

            auto* move = ball->GetMovement();
            float ballRadius = ball->GetRadius();

            XMFLOAT3 currentPos = move->GetPosition();
            XMFLOAT3 currentVel = ball->GetVelocity();
            XMFLOAT3 targetPos = ball->PredictNextPosition(elapsedTime);

            // ============================================
            // STEP 3: Multi-Pass Collision Detection
            // ============================================
            bool hitWall = false;
            float closestHitTime = 2.0f;
            XMFLOAT3 wallHitNormal = { 0, 0, 0 };
            XMFLOAT3 wallHitPoint = currentPos;

            if (m_stage)
            {
                // ? OPTIMIZATION: Spatial query instead of checking ALL walls
                // Calculate query radius (current pos to target pos + ball radius)
                float dx = targetPos.x - currentPos.x;
                float dz = targetPos.z - currentPos.z;
                float travelDist = std::sqrt(dx * dx + dz * dz);
                float queryRadius = travelDist + ballRadius + 2.0f;

                // Get only nearby walls
                auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(currentPos, queryRadius);

                // ? MASSIVE OPTIMIZATION: Instead of looping 2000 walls,
                // we now only check 5-20 nearby walls!
                for (size_t wallIdx : nearbyWallIndices)
                {
                    const auto& wall = m_stage->m_debugWalls[wallIdx];
                
                    // ? OPTIMIZATION: Broad Phase Culling
                    // If the ball is too far from this segment, SKIP IT immediately.
                    // This prevents the expensive TransformToLocal matrix math below.

                    float dx = currentPos.x - wall.Position.x;
                    float dz = currentPos.z - wall.Position.z;

                    // Simple squared distance check (fast)
                    float distSq = (dx * dx) + (dz * dz);

                    // Calculate safe threshold: WallRadius + BallRadius + SafetyMargin
                    // We add a little extra (e.g. 2.0f) to account for high-speed movement
                    float checkRadius = wall.WorldRadius + ballRadius + 2.0f;

                    if (distSq > (checkRadius * checkRadius))
                    {
                        continue; // Skip this wall, it's too far away!
                    }

                    // Wall AABB (Rough approximation)
                    float maxScale = (std::max)({ wall.Scale.x, wall.Scale.y, wall.Scale.z });
                    float wMinX = wall.Position.x - maxScale;
                    float wMaxX = wall.Position.x + maxScale;
                    float wMinZ = wall.Position.z - maxScale;
                    float wMaxZ = wall.Position.z + maxScale;

                    // Block AABB
                    float bMinX = currentPos.x - ballRadius;
                    float bMaxX = currentPos.x + ballRadius;
                    float bMinZ = currentPos.z - ballRadius;
                    float bMaxZ = currentPos.z + ballRadius;

                    // AABB Intersection Test
                    if (bMaxX < wMinX || bMinX > wMaxX || bMaxZ < wMinZ || bMinZ > wMaxZ)
                    {
                        continue; // Skip OBB if boxes don't touch
                    }
              
                    // ? Transform to local space
                    XMVECTOR vLocStart = TransformToLocal(currentPos, wall);
                    XMVECTOR vLocEnd = TransformToLocal(targetPos, wall);

                    XMFLOAT3 p0, p1;
                    XMStoreFloat3(&p0, vLocStart);
                    XMStoreFloat3(&p1, vLocEnd);

                    // ? CRITICAL: Add safety margin + skin width
                    float halfX = wall.Scale.x * 0.5f;
                    float halfZ = wall.Scale.z * 0.5f;

                    float boxMinX = -halfX - ballRadius - SKIN_WIDTH;
                    float boxMaxX = halfX + ballRadius + SKIN_WIDTH;
                    float boxMinZ = -halfZ - ballRadius - SKIN_WIDTH;
                    float boxMaxZ = halfZ + ballRadius + SKIN_WIDTH;

                    // ? Calculate ray direction
                    float dirX = p1.x - p0.x;
                    float dirZ = p1.z - p0.z;

                    // ? Calculate ray length for normalization check
                    float rayLengthSq = dirX * dirX + dirZ * dirZ;

                    // ? Skip if ray is too short (ball barely moved)
                    if (rayLengthSq < 1e-8f) {
                        // Static collision check instead
                        if (p0.x >= boxMinX && p0.x <= boxMaxX &&
                            p0.z >= boxMinZ && p0.z <= boxMaxZ)
                        {
                            // Ball is inside - push out to nearest face
                            float distToMinX = p0.x - boxMinX;
                            float distToMaxX = boxMaxX - p0.x;
                            float distToMinZ = p0.z - boxMinZ;
                            float distToMaxZ = boxMaxZ - p0.z;

                            float minDist = (std::min)({ distToMinX, distToMaxX, distToMinZ, distToMaxZ });

                            XMFLOAT3 localNormal = { 0, 0, 0 };
                            if (minDist == distToMinX)      localNormal = { -1, 0, 0 };
                            else if (minDist == distToMaxX) localNormal = { 1, 0, 0 };
                            else if (minDist == distToMinZ) localNormal = { 0, 0, -1 };
                            else                            localNormal = { 0, 0, 1 };

                            closestHitTime = 0.0f;

                            XMVECTOR vNormalLocal = XMLoadFloat3(&localNormal);
                            XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
                                XMConvertToRadians(wall.Rotation.x),
                                XMConvertToRadians(wall.Rotation.y),
                                XMConvertToRadians(wall.Rotation.z)
                            );
                            XMVECTOR vNormalWorld = XMVector3TransformNormal(vNormalLocal, matRot);
                            XMStoreFloat3(&wallHitNormal, XMVector3Normalize(vNormalWorld));

                            wallHitPoint = currentPos;
                            hitWall = true;
                        }
                        continue;
                    }

                    // ? Slab method with enhanced edge detection
                    float tMin = 0.0f;
                    float tMax = 1.0f;
                    XMFLOAT3 hitNormalLocal = { 0, 0, 0 };

                    // --- X-axis slab ---
                    if (fabsf(dirX) > PARALLEL_EPSILON)
                    {
                        float invDirX = 1.0f / dirX;
                        float t1 = (boxMinX - p0.x) * invDirX;
                        float t2 = (boxMaxX - p0.x) * invDirX;

                        if (t1 > t2) std::swap(t1, t2);

                        if (t1 > tMin)
                        {
                            tMin = t1;
                            hitNormalLocal = (dirX > 0) ? XMFLOAT3{ -1, 0, 0 } : XMFLOAT3{ 1, 0, 0 };
                        }
                        if (t2 < tMax) tMax = t2;
                    }
                    else
                    {
                        // ? CRITICAL: Use relaxed bounds check for parallel rays
                        if (p0.x < boxMinX - PARALLEL_EPSILON ||
                            p0.x > boxMaxX + PARALLEL_EPSILON)
                        {
                            continue;
                        }
                    }

                    if (tMin > tMax) continue;

                    // --- Z-axis slab ---
                    if (fabsf(dirZ) > PARALLEL_EPSILON)
                    {
                        float invDirZ = 1.0f / dirZ;
                        float t1 = (boxMinZ - p0.z) * invDirZ;
                        float t2 = (boxMaxZ - p0.z) * invDirZ;

                        if (t1 > t2) std::swap(t1, t2);

                        if (t1 > tMin)
                        {
                            tMin = t1;
                            hitNormalLocal = (dirZ > 0) ? XMFLOAT3{ 0, 0, -1 } : XMFLOAT3{ 0, 0, 1 };
                        }
                        if (t2 < tMax) tMax = t2;
                    }
                    else
                    {
                        // ? CRITICAL: Use relaxed bounds check
                        if (p0.z < boxMinZ - PARALLEL_EPSILON ||
                            p0.z > boxMaxZ + PARALLEL_EPSILON)
                        {
                            continue;
                        }
                    }

                    if (tMin > tMax) continue;

                    // ? CRITICAL: Allow slight back-penetration for edge cases
                    if (tMin >= MIN_HIT_TIME && tMin <= 1.0f && tMin < closestHitTime)
                    {
                        // ? Additional validation: Ensure hit point is actually on surface
                        XMFLOAT3 testHitLocal;
                        testHitLocal.x = p0.x + dirX * tMin;
                        testHitLocal.y = 0.0f;
                        testHitLocal.z = p0.z + dirZ * tMin;

                        // ? Verify hit point is within box bounds (with tolerance)
                        const float BOUNDS_TOLERANCE = 0.05f;
                        bool validHit = (
                            testHitLocal.x >= boxMinX - BOUNDS_TOLERANCE &&
                            testHitLocal.x <= boxMaxX + BOUNDS_TOLERANCE &&
                            testHitLocal.z >= boxMinZ - BOUNDS_TOLERANCE &&
                            testHitLocal.z <= boxMaxZ + BOUNDS_TOLERANCE
                            );

                        if (!validHit) continue;

                        closestHitTime = tMin;

                        // Transform normal to world space
                        XMVECTOR vNormalLocal = XMLoadFloat3(&hitNormalLocal);
                        XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
                            XMConvertToRadians(wall.Rotation.x),
                            XMConvertToRadians(wall.Rotation.y),
                            XMConvertToRadians(wall.Rotation.z)
                        );
                        XMVECTOR vNormalWorld = XMVector3TransformNormal(vNormalLocal, matRot);
                        vNormalWorld = XMVector3Normalize(vNormalWorld);
                        XMStoreFloat3(&wallHitNormal, vNormalWorld);

                        XMVECTOR vStart = XMLoadFloat3(&currentPos);
                        XMVECTOR vEnd = XMLoadFloat3(&targetPos);
                        XMVECTOR vHitPoint = XMVectorLerp(vStart, vEnd, tMin);
                        XMStoreFloat3(&wallHitPoint, vHitPoint);

                        hitWall = true;
                    }
                }
            }

            // ============================================
            // STEP 4: Handle Wall Collision
            // ============================================
            if (hitWall)
            {
                if (type == AttackType::Static)
                {
                    it = projectiles.erase(it);
                    continue;
                }
                else
                {
                    // ? Enhanced bounce calculation
                    XMVECTOR vCur = XMLoadFloat3(&currentPos);
                    XMVECTOR vTar = XMLoadFloat3(&targetPos);
                    XMVECTOR vDir = XMVectorSubtract(vTar, vCur);

                    // ? CRITICAL: Use clamped safe time
                    float safeT = (std::max)(0.0f, closestHitTime * 0.95f);
                    XMVECTOR vSafePos = XMVectorAdd(vCur, XMVectorScale(vDir, safeT));

                    XMFLOAT3 safePos;
                    XMStoreFloat3(&safePos, vSafePos);
                    safePos.y = 0.0f;

                    // Reflect velocity
                    XMVECTOR vVel = XMLoadFloat3(&currentVel);
                    XMVECTOR vNorm = XMLoadFloat3(&wallHitNormal);
                    XMVECTOR vReflected = XMVector3Reflect(vVel, vNorm);
                    vReflected = XMVectorSetY(vReflected, 0.0f);

                    XMFLOAT3 reflectedVel;
                    XMStoreFloat3(&reflectedVel, vReflected);

                    // Calculate remaining movement
                    float remainingT = (std::max)(0.0f, 1.0f - closestHitTime);
                    float remainingTime = remainingT * elapsedTime;

                    XMFLOAT3 finalPos = safePos;
                    finalPos.x += reflectedVel.x * remainingTime;
                    finalPos.z += reflectedVel.z * remainingTime;

                    // ? CRITICAL: Stronger push-out from surface
                    finalPos.x += wallHitNormal.x * (ballRadius * 0.2f + SKIN_WIDTH);
                    finalPos.z += wallHitNormal.z * (ballRadius * 0.2f + SKIN_WIDTH);
                    finalPos.y = 0.0f;

                    ball->ApplyMovement(finalPos, reflectedVel);
                    ball->UpdatePreviousPosition();

                    ++it;
                    continue;
                }
            }

            // No wall hit - apply normal movement
            ball->ApplyMovement(targetPos, currentVel);
            ball->UpdatePreviousPosition();

            // Check block collision
            if (m_blockManager)
            {
                if (m_blockManager->CheckEnemyCollision(ball.get()))
                {
                    it = projectiles.erase(it);
                    continue;
                }
            }

            ++it;
        }
    }
}

// =========================================================
// PLAYER VS DEBUG WALLS (ROBUST OBB)
// =========================================================
void CollisionManager::CheckStageCollision()
{
    if (!m_player || !m_stage) return;

    float playerRadius = 0.5f;
    auto* moveComp = m_player->GetMovement();

    int iterations = 4;

    for (int iter = 0; iter < iterations; ++iter)
    {
        XMFLOAT3 playerPos = moveComp->GetPosition();
        XMFLOAT3 vel = moveComp->GetVelocity();
        bool collidedAny = false;

        // =========================================================
        // THIS LOOP IS THE PERFORMANCE KILLER
        // =========================================================
        for (const auto& wall : m_stage->m_debugWalls)
        {
            // ? INSERT THIS OPTIMIZATION HERE -----------------------
            // Calculate distance between Player and Wall Center
            float dx = playerPos.x - wall.Position.x;
            float dz = playerPos.z - wall.Position.z;
            float distSq = (dx * dx) + (dz * dz);

            // Calculate a safe "Check Radius"
            // If the player is further than this, we don't need to do OBB math.
            // (Wall Radius + Player Radius + little extra margin)
            float checkRadius = wall.WorldRadius + playerRadius + 2.0f;

            // If too far, SKIP the expensive math below
            if (distSq > (checkRadius * checkRadius))
            {
                continue;
            }
            // --------------------------------------------------------

            DebugWallData solidWall = wall;
            solidWall.Scale.y = 1000.0f;

            XMFLOAT3 fixPos = playerPos;
            if (Collision::ResolveOBB(playerPos, playerRadius, solidWall, fixPos))
            {
                float dx = fixPos.x - playerPos.x;
                float dy = fixPos.y - playerPos.y;
                float dz = fixPos.z - playerPos.z;

                playerPos = fixPos;
                collidedAny = true;

                XMVECTOR vPush = XMVectorSet(dx, dy, dz, 0.0f);
                XMVECTOR vVel = XMLoadFloat3(&vel);
                XMVECTOR vNormal = XMVector3Normalize(vPush);
                float dot = XMVectorGetX(XMVector3Dot(vVel, vNormal));

                if (dot < 0.0f)
                {
                    vVel = XMVectorSubtract(vVel, XMVectorScale(vNormal, dot));
                    XMStoreFloat3(&vel, vVel);
                }
            }
        }

        if (collidedAny)
        {
            moveComp->SetPosition(playerPos);
            moveComp->SetVelocity(vel);
        }
    }
}

// =========================================================
// BLOCKS VS DEBUG WALLS (ROBUST OBB)
// =========================================================
void CollisionManager::CheckBlockVsStage()
{
    if (!m_blockManager || !m_stage) return;

    float blockRadius = 0.5f;
    int iterations = 4;

    auto& blocks = m_blockManager->GetBlocks();
    for (auto& block : blocks)
    {
        if (!block.IsActive()) continue; 
        block.ResetCollisionState();     

        auto* moveComp = block.GetMovement(); // 

        for (int iter = 0; iter < iterations; ++iter)
        {
            XMFLOAT3 blockPos = moveComp->GetPosition();
            XMFLOAT3 vel = moveComp->GetVelocity();
            bool collidedAny = false;

            // ? SPATIAL HASH OPTIMIZATION
            // Query spatial grid to get only nearby walls
            float queryRadius = blockRadius + 3.0f; // Block radius + margin
            auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(blockPos, queryRadius);

            // ? NOW: Only check ~5-15 walls instead of 2000!
            for (size_t wallIdx : nearbyWallIndices)
            {
                const auto& wall = m_stage->m_debugWalls[wallIdx];

                // Secondary culling (optional, but good practice)
                float dx = blockPos.x - wall.Position.x;
                float dz = blockPos.z - wall.Position.z;
                float distSq = (dx * dx) + (dz * dz);
                float checkRadius = wall.WorldRadius + blockRadius + 2.0f;

                if (distSq > (checkRadius * checkRadius))
                {
                    continue;
                }

                DebugWallData solidWall = wall;
                solidWall.Scale.y = 1000.0f;

                XMFLOAT3 fixPos = blockPos;
                if (Collision::ResolveOBB(blockPos, blockRadius, solidWall, fixPos))
                {
                    float dx = fixPos.x - blockPos.x;
                    float dy = fixPos.y - blockPos.y;
                    float dz = fixPos.z - blockPos.z;

                    blockPos = fixPos;
                    collidedAny = true;
                    block.SetWallCollision({ 0, 1, 0 });

                    XMVECTOR vPush = XMVectorSet(dx, dy, dz, 0.0f);
                    XMVECTOR vVel = XMLoadFloat3(&vel);
                    XMVECTOR vNormal = XMVector3Normalize(vPush);
                    float dot = XMVectorGetX(XMVector3Dot(vVel, vNormal));

                    if (dot < 0.0f)
                    {
                        vVel = XMVectorSubtract(vVel, XMVectorScale(vNormal, dot));
                        XMStoreFloat3(&vel, vVel);
                    }
                }
            }

            if (collidedAny)
            {
                moveComp->SetPosition(blockPos);
                moveComp->SetVelocity(vel);
            }
        }
    }
}

// =========================================================
// (UNCHANGED) Block vs Block Logic
// =========================================================
void CollisionManager::UpdateBlockStackFlags()
{
    if (!m_blockManager) return;
    float connectDist = 0.95f;
    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();

    for (size_t i = 0; i < count; ++i)
    {
        if (!blocks[i].IsActive() || blocks[i].IsFilling()) continue;
        for (size_t j = i + 1; j < count; ++j)
        {
            if (!blocks[j].IsActive() || blocks[j].IsFilling()) continue;
            auto* movA = blocks[i].GetMovement();
            auto* movB = blocks[j].GetMovement();
            XMFLOAT3 posA = movA->GetPosition();
            XMFLOAT3 posB = movB->GetPosition();
            float dx = std::abs(posA.x - posB.x);
            float dz = std::abs(posA.z - posB.z);

            if (dx < connectDist && dz < connectDist)
            {
                bool a_is_anchor = blocks[i].IsStacked(); // 
                bool b_is_anchor = blocks[j].IsStacked(); // 
                if (a_is_anchor && !b_is_anchor) {
                    blocks[j].SetStacked(true);
                    movB->SetVelocityX(0.0f); movB->SetVelocityZ(0.0f);
                }
                else if (!a_is_anchor && b_is_anchor) {
                    blocks[i].SetStacked(true);
                    movA->SetVelocityX(0.0f); movA->SetVelocityZ(0.0f);
                }
            }
        }
    }
}

void CollisionManager::CheckBlockVsBlocks()
{
    if (!m_blockManager) return;

    float boxRadius = 0.4f;
    float minTouchDist = boxRadius * 2.45f;
    float pushTarget = minTouchDist + 0.001f;
    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();
    int physicsIterations = 4;

    // ? BUILD TEMPORARY SPATIAL GRID FOR BLOCKS
    // This is rebuilt every frame (cheap for hundreds of blocks)
    m_blockGrid.Clear();

    for (size_t i = 0; i < count; ++i)
    {
        if (!blocks[i].IsActive() || blocks[i].IsFilling()) continue;

        XMFLOAT3 pos = blocks[i].GetMovement()->GetPosition();
        m_blockGrid.Insert(pos, boxRadius, i);
    }

    for (int k = 0; k < physicsIterations; ++k)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (!blocks[i].IsActive() || blocks[i].IsFilling()) continue;

            auto* movA = blocks[i].GetMovement();
            XMFLOAT3 posA = movA->GetPosition();

            // ? SPATIAL QUERY: Only get blocks within interaction range
            float queryRadius = minTouchDist + 0.5f;
            auto nearbyBlockIndices = m_blockGrid.QueryRadius(posA, queryRadius);

            // ? NOW: Only check ~5-10 blocks instead of 200!
            for (size_t j : nearbyBlockIndices)
            {
                // Skip self and already-processed pairs
                if (j <= i) continue;
                if (!blocks[j].IsActive() || blocks[j].IsFilling()) continue;

                auto* movB = blocks[j].GetMovement();
                XMFLOAT3 posB = movB->GetPosition();

                float dx = std::abs(posA.x - posB.x);
                float dz = std::abs(posA.z - posB.z);

                if (dx < minTouchDist && dz < minTouchDist)
                {
                    float overlapX = pushTarget - dx;
                    float overlapZ = pushTarget - dz;
                    bool useX = (overlapX < overlapZ);
                    float moveAmt = useX ? overlapX : overlapZ;
                    float dir = 0.0f;
                    if (useX) dir = (posA.x < posB.x) ? -1.0f : 1.0f;
                    else      dir = (posA.z < posB.z) ? -1.0f : 1.0f;

                    bool a_anchor = blocks[i].IsStacked(); 
                    bool b_anchor = blocks[j].IsStacked(); 

                    if (a_anchor && !b_anchor) {
                        posB = movB->GetPosition(); // Refresh position
                        if (useX) posB.x -= dir * moveAmt;
                        else posB.z -= dir * moveAmt;
                        movB->SetPosition(posB);
                    }
                    else if (!a_anchor && b_anchor) {
                        posA = movA->GetPosition(); // Refresh position
                        if (useX) posA.x += dir * moveAmt;
                        else posA.z += dir * moveAmt;
                        movA->SetPosition(posA);
                    }
                    else {
                        // Refresh both positions
                        posA = movA->GetPosition();
                        posB = movB->GetPosition();

                        float half = moveAmt * 0.5f;
                        if (useX) {
                            posA.x += dir * half;
                            posB.x -= dir * half;
                        }
                        else {
                            posA.z += dir * half;
                            posB.z -= dir * half;
                        }
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

    for (int k = 0; k < 2; ++k)
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (!block.IsActive() || block.IsFilling()) continue;
            XMFLOAT3 blockPos = block.GetMovement()->GetPosition();
            XMFLOAT3 outPos;
            if (Collision::IntersectCubeVsCube(blockPos, blockSize, playerPos, playerSize, outPos))
            {
                playerPos = outPos;
                playerMove->SetPosition(playerPos);
                float dx = playerPos.x - blockPos.x;
                float dz = playerPos.z - blockPos.z;
                if (std::abs(dx) > std::abs(dz)) {
                    if ((dx > 0 && playerVel.x < 0) || (dx < 0 && playerVel.x > 0)) playerMove->SetVelocityX(0.0f);
                }
                else {
                    if ((dz > 0 && playerVel.z < 0) || (dz < 0 && playerVel.z > 0)) playerMove->SetVelocityZ(0.0f);
                }
                playerVel = playerMove->GetVelocity();
            }
        }
    }
}