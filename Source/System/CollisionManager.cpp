#include "CollisionManager.h"

using namespace DirectX;

// =========================================================
// HELPER FUNCTIONS (UNCHANGED)
// =========================================================
static XMVECTOR TransformToLocal(const XMFLOAT3& worldPos, const DebugWallData& wall)
{
    XMVECTOR vWorldPos = XMLoadFloat3(&worldPos);
    XMVECTOR vWallPos = XMLoadFloat3(&wall.Position);
    XMVECTOR vRelative = XMVectorSubtract(vWorldPos, vWallPos);
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );
    XMMATRIX matInvRot = XMMatrixTranspose(matRot);
    return XMVector3TransformNormal(vRelative, matInvRot);
}

static XMVECTOR TransformToWorld(const XMVECTOR& localVec, const DebugWallData& wall)
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );
    return XMVector3TransformNormal(localVec, matRot);
}

static XMVECTOR TransformNormalToWorld(const XMVECTOR& localNorm, const DebugWallData& wall)
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );
    return XMVector3TransformNormal(localNorm, matRot);
}

static XMVECTOR TransformToEnemyLocal(const XMFLOAT3& worldPos, const Enemy* enemy)
{
    XMVECTOR vWorldPos = XMLoadFloat3(&worldPos);
    XMVECTOR vEnemyPos = XMLoadFloat3(&enemy->GetPosition());
    XMVECTOR vRelative = XMVectorSubtract(vWorldPos, vEnemyPos);
    XMFLOAT3 rot = enemy->GetRotation();
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
    XMMATRIX matInvRot = XMMatrixTranspose(matRot);
    return XMVector3TransformNormal(vRelative, matInvRot);
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

void CollisionManager::Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em, ItemManager* im)
{
    m_player = p;
    m_stage = s;
    m_blockManager = bm;
    m_enemyManager = em;
    m_itemManager = im;
}

void CollisionManager::Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em, ItemManager* im, Boss* boss)
{
    m_player = p;
    m_stage = s;
    m_blockManager = bm;
    m_enemyManager = em;
    m_itemManager = im;
    m_boss = boss; // Simpan pointer boss
}

void CollisionManager::Update(float elapsedTime)
{
    if (!m_player) return;

    m_player->GetMovement()->SetGravityEnabled(false);
    m_player->GetMovement()->SetVelocityY(0.0f);

    if (m_blockManager)
    {
        for (auto& blockPtr : m_blockManager->GetBlocks())
        {
            if (blockPtr->IsActive())
            {
                if (!blockPtr->IsFalling())
                {
                    blockPtr->GetMovement()->SetGravityEnabled(false);
                    blockPtr->GetMovement()->SetVelocityY(0.0f);
                }
            }
        }
    }

    if (m_player->GetGameStage() != 3) return;

    CheckEnemyProjectilesFull(elapsedTime);
    CheckStageCollision();
    CheckBlockVsStage();
    CheckBlockVsVoidLines();
    CheckPlayerVsEnemies();
    CheckBossFilesVsPlayer();

    if (m_blockManager)
    {
        int propagationSteps = 15;
        for (int i = 0; i < propagationSteps; ++i)
        {
            UpdateBlockStackFlags();
        }
        CheckBlockVsBlocks();
        CheckPlayerVsBlocks();
        CheckBlockVsEnemies();
        CheckBossFilesVsBlocks();
    }

    if (m_itemManager)
    {
        CheckPlayerVsItems();
        CheckBlockVsItems();
    }
}

void CollisionManager::CheckEnemyProjectilesFull(float elapsedTime)
{
    if (!m_enemyManager) return;

    static const float PARALLEL_EPSILON = 1e-4f;
    static const float SKIN_WIDTH = 0.01f;
    static const float MIN_HIT_TIME = -0.001f;

    for (auto& enemy : m_enemyManager->GetEnemies())
    {
        auto& projectiles = enemy->GetProjectiles();
        AttackType type = enemy->GetAttackType();

        for (auto it = projectiles.begin(); it != projectiles.end(); )
        {
            auto& ball = *it;
            if (!ball) { ++it; continue; }

            auto* move = ball->GetMovement();
            float ballRadius = ball->GetRadius();

            XMFLOAT3 currentPos = move->GetPosition();
            XMFLOAT3 currentVel = ball->GetVelocity();
            XMFLOAT3 targetPos = ball->PredictNextPosition(elapsedTime);

            bool hitWall = false;
            float closestHitTime = 2.0f;
            XMFLOAT3 wallHitNormal = { 0, 0, 0 };
            XMFLOAT3 wallHitPoint = currentPos;

            if (m_stage)
            {
                float dx = targetPos.x - currentPos.x;
                float dz = targetPos.z - currentPos.z;
                float travelDist = std::sqrt(dx * dx + dz * dz);
                float queryRadius = travelDist + ballRadius + 2.0f;
                auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(currentPos, queryRadius);

                for (size_t wallIdx : nearbyWallIndices)
                {
                    const auto& wall = m_stage->m_debugWalls[wallIdx];
                    float dx = currentPos.x - wall.Position.x;
                    float dz = currentPos.z - wall.Position.z;
                    float distSq = (dx * dx) + (dz * dz);
                    float checkRadius = wall.WorldRadius + ballRadius + 2.0f;

                    if (distSq > (checkRadius * checkRadius)) continue;

                    float maxScale = (std::max)({ wall.Scale.x, wall.Scale.y, wall.Scale.z });
                    float wMinX = wall.Position.x - maxScale;
                    float wMaxX = wall.Position.x + maxScale;
                    float wMinZ = wall.Position.z - maxScale;
                    float wMaxZ = wall.Position.z + maxScale;

                    float bMinX = currentPos.x - ballRadius;
                    float bMaxX = currentPos.x + ballRadius;
                    float bMinZ = currentPos.z - ballRadius;
                    float bMaxZ = currentPos.z + ballRadius;

                    if (bMaxX < wMinX || bMinX > wMaxX || bMaxZ < wMinZ || bMinZ > wMaxZ) continue;

                    XMVECTOR vLocStart = TransformToLocal(currentPos, wall);
                    XMVECTOR vLocEnd = TransformToLocal(targetPos, wall);

                    XMFLOAT3 p0, p1;
                    XMStoreFloat3(&p0, vLocStart);
                    XMStoreFloat3(&p1, vLocEnd);

                    float halfX = wall.Scale.x * 0.5f;
                    float halfZ = wall.Scale.z * 0.5f;
                    float boxMinX = -halfX - ballRadius - SKIN_WIDTH;
                    float boxMaxX = halfX + ballRadius + SKIN_WIDTH;
                    float boxMinZ = -halfZ - ballRadius - SKIN_WIDTH;
                    float boxMaxZ = halfZ + ballRadius + SKIN_WIDTH;

                    float dirX = p1.x - p0.x;
                    float dirZ = p1.z - p0.z;
                    float rayLengthSq = dirX * dirX + dirZ * dirZ;

                    if (rayLengthSq < 1e-8f) {
                        if (p0.x >= boxMinX && p0.x <= boxMaxX &&
                            p0.z >= boxMinZ && p0.z <= boxMaxZ)
                        {
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
                            XMVECTOR vNormalWorld = TransformNormalToWorld(vNormalLocal, wall);
                            XMStoreFloat3(&wallHitNormal, XMVector3Normalize(vNormalWorld));
                            wallHitPoint = currentPos;
                            hitWall = true;
                        }
                        continue;
                    }

                    float tMin = 0.0f;
                    float tMax = 1.0f;
                    XMFLOAT3 hitNormalLocal = { 0, 0, 0 };

                    if (fabsf(dirX) > PARALLEL_EPSILON)
                    {
                        float invDirX = 1.0f / dirX;
                        float t1 = (boxMinX - p0.x) * invDirX;
                        float t2 = (boxMaxX - p0.x) * invDirX;
                        if (t1 > t2) std::swap(t1, t2);
                        if (t1 > tMin) { tMin = t1; hitNormalLocal = (dirX > 0) ? XMFLOAT3{ -1, 0, 0 } : XMFLOAT3{ 1, 0, 0 }; }
                        if (t2 < tMax) tMax = t2;
                    }
                    else if (p0.x < boxMinX - PARALLEL_EPSILON || p0.x > boxMaxX + PARALLEL_EPSILON) continue;

                    if (tMin > tMax) continue;

                    if (fabsf(dirZ) > PARALLEL_EPSILON)
                    {
                        float invDirZ = 1.0f / dirZ;
                        float t1 = (boxMinZ - p0.z) * invDirZ;
                        float t2 = (boxMaxZ - p0.z) * invDirZ;
                        if (t1 > t2) std::swap(t1, t2);
                        if (t1 > tMin) { tMin = t1; hitNormalLocal = (dirZ > 0) ? XMFLOAT3{ 0, 0, -1 } : XMFLOAT3{ 0, 0, 1 }; }
                        if (t2 < tMax) tMax = t2;
                    }
                    else if (p0.z < boxMinZ - PARALLEL_EPSILON || p0.z > boxMaxZ + PARALLEL_EPSILON) continue;

                    if (tMin > tMax) continue;

                    if (tMin >= MIN_HIT_TIME && tMin <= 1.0f && tMin < closestHitTime)
                    {
                        XMFLOAT3 testHitLocal;
                        testHitLocal.x = p0.x + dirX * tMin;
                        testHitLocal.y = 0.0f;
                        testHitLocal.z = p0.z + dirZ * tMin;

                        const float BOUNDS_TOLERANCE = 0.05f;
                        bool validHit = (
                            testHitLocal.x >= boxMinX - BOUNDS_TOLERANCE &&
                            testHitLocal.x <= boxMaxX + BOUNDS_TOLERANCE &&
                            testHitLocal.z >= boxMinZ - BOUNDS_TOLERANCE &&
                            testHitLocal.z <= boxMaxZ + BOUNDS_TOLERANCE
                            );

                        if (!validHit) continue;

                        closestHitTime = tMin;
                        XMVECTOR vNormalLocal = XMLoadFloat3(&hitNormalLocal);
                        XMVECTOR vNormalWorld = TransformNormalToWorld(vNormalLocal, wall);
                        XMStoreFloat3(&wallHitNormal, XMVector3Normalize(vNormalWorld));

                        XMVECTOR vStart = XMLoadFloat3(&currentPos);
                        XMVECTOR vEnd = XMLoadFloat3(&targetPos);
                        XMVECTOR vHitPoint = XMVectorLerp(vStart, vEnd, tMin);
                        XMStoreFloat3(&wallHitPoint, vHitPoint);
                        hitWall = true;
                    }
                }
            }

            if (hitWall)
            {
                if (type == AttackType::Static) {
                    it = projectiles.erase(it);
                    continue;
                }
                else {
                    XMVECTOR vCur = XMLoadFloat3(&currentPos);
                    XMVECTOR vTar = XMLoadFloat3(&targetPos);
                    XMVECTOR vDir = XMVectorSubtract(vTar, vCur);
                    float safeT = (std::max)(0.0f, closestHitTime * 0.95f);
                    XMVECTOR vSafePos = XMVectorAdd(vCur, XMVectorScale(vDir, safeT));

                    XMFLOAT3 safePos;
                    XMStoreFloat3(&safePos, vSafePos);
                    safePos.y = 0.0f;

                    XMVECTOR vVel = XMLoadFloat3(&currentVel);
                    XMVECTOR vNorm = XMLoadFloat3(&wallHitNormal);
                    XMVECTOR vReflected = XMVector3Reflect(vVel, vNorm);
                    vReflected = XMVectorSetY(vReflected, 0.0f);

                    XMFLOAT3 reflectedVel;
                    XMStoreFloat3(&reflectedVel, vReflected);

                    float remainingT = (std::max)(0.0f, 1.0f - closestHitTime);
                    float remainingTime = remainingT * elapsedTime;

                    XMFLOAT3 finalPos = safePos;
                    finalPos.x += reflectedVel.x * remainingTime;
                    finalPos.z += reflectedVel.z * remainingTime;
                    finalPos.x += wallHitNormal.x * (ballRadius * 0.2f + SKIN_WIDTH);
                    finalPos.z += wallHitNormal.z * (ballRadius * 0.2f + SKIN_WIDTH);
                    finalPos.y = 0.0f;

                    ball->ApplyMovement(finalPos, reflectedVel);
                    ball->UpdatePreviousPosition();

                    ++it;
                    continue;
                }
            }

            ball->ApplyMovement(targetPos, currentVel);
            ball->UpdatePreviousPosition();

            if (m_blockManager) {
                if (m_blockManager->CheckEnemyCollision(ball.get())) {
                    it = projectiles.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

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

        for (const auto& wall : m_stage->m_debugWalls)
        {
            float dx = playerPos.x - wall.Position.x;
            float dz = playerPos.z - wall.Position.z;
            float distSq = (dx * dx) + (dz * dz);
            float checkRadius = wall.WorldRadius + playerRadius + 2.0f;

            if (distSq > (checkRadius * checkRadius)) continue;

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
                if (dot < 0.0f) {
                    vVel = XMVectorSubtract(vVel, XMVectorScale(vNormal, dot));
                    XMStoreFloat3(&vel, vVel);
                }
            }
        }
        if (collidedAny) {
            moveComp->SetPosition(playerPos);
            moveComp->SetVelocity(vel);
        }
    }
}

void CollisionManager::CheckBlockVsStage()
{
    if (!m_blockManager || !m_stage) return;

    float blockRadius = 0.5f;
    int iterations = 4;

    auto& blocks = m_blockManager->GetBlocks();
    for (auto& blockPtr : blocks)
    {
        // Dereference unique_ptr
        Block& block = *blockPtr;

        if (!block.IsActive()) continue;
        block.ResetCollisionState();

        auto* moveComp = block.GetMovement();

        if (block.IsProjectile())
        {
            XMFLOAT3 blockPos = moveComp->GetPosition();
            float queryRadius = blockRadius + 3.0f;
            auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(blockPos, queryRadius);

            for (size_t wallIdx : nearbyWallIndices)
            {
                const auto& wall = m_stage->m_debugWalls[wallIdx];
                float dx = blockPos.x - wall.Position.x;
                float dz = blockPos.z - wall.Position.z;
                float distSq = (dx * dx) + (dz * dz);
                float checkRadius = wall.WorldRadius + blockRadius + 1.0f;

                if (distSq > (checkRadius * checkRadius)) continue;

                DebugWallData solidWall = wall;
                solidWall.Scale.y = 1000.0f;
                XMFLOAT3 fixPos = blockPos;

                if (Collision::ResolveOBB(blockPos, blockRadius, solidWall, fixPos))
                {
                    block.OnHit();
                    break;
                }
            }
            continue;
        }

        for (int iter = 0; iter < iterations; ++iter)
        {
            XMFLOAT3 blockPos = moveComp->GetPosition();
            XMFLOAT3 vel = moveComp->GetVelocity();
            bool collidedAny = false;

            float queryRadius = blockRadius + 3.0f;
            auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(blockPos, queryRadius);

            for (size_t wallIdx : nearbyWallIndices)
            {
                const auto& wall = m_stage->m_debugWalls[wallIdx];
                float dx = blockPos.x - wall.Position.x;
                float dz = blockPos.z - wall.Position.z;
                float distSq = (dx * dx) + (dz * dz);
                float checkRadius = wall.WorldRadius + blockRadius + 2.0f;

                if (distSq > (checkRadius * checkRadius)) continue;

                DebugWallData solidWall = wall;
                solidWall.Scale.y = 1000.0f;
                XMFLOAT3 fixPos = blockPos;
                if (Collision::ResolveOBB(blockPos, blockRadius, solidWall, fixPos))
                {
                    if (m_blockManager->IsInvincible())
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
                        if (dot < 0.0f) {
                            vVel = XMVectorSubtract(vVel, XMVectorScale(vNormal, dot));
                            XMStoreFloat3(&vel, vVel);
                        }
                    }
                    else {
                        block.OnHit();
                        break;
                    }
                }
            }
            if (collidedAny) {
                moveComp->SetPosition(blockPos);
                moveComp->SetVelocity(vel);
            }
        }
    }
}

void CollisionManager::UpdateBlockStackFlags()
{
    if (!m_blockManager) return;
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

            if (dx < connectDist && dz < connectDist)
            {
                bool a_is_anchor = blocks[i]->IsStacked();
                bool b_is_anchor = blocks[j]->IsStacked();
                if (a_is_anchor && !b_is_anchor) {
                    blocks[j]->SetStacked(true);
                    movB->SetVelocityX(0.0f); movB->SetVelocityZ(0.0f);
                }
                else if (!a_is_anchor && b_is_anchor) {
                    blocks[i]->SetStacked(true);
                    movA->SetVelocityX(0.0f); movA->SetVelocityZ(0.0f);
                }
            }
        }
    }
}

void CollisionManager::CheckBlockVsBlocks()
{
    if (!m_blockManager) return;
    if (m_blockManager->IsShieldActive()) return;

    float boxRadius = 0.4f;
    float minTouchDist = boxRadius * 2.45f;
    float pushTarget = minTouchDist + 0.001f;
    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();
    int physicsIterations = 4;

    m_blockGrid.Clear();

    for (size_t i = 0; i < count; ++i)
    {
        if (!blocks[i]->IsActive() || blocks[i]->IsFilling()) continue;
        XMFLOAT3 pos = blocks[i]->GetMovement()->GetPosition();
        m_blockGrid.Insert(pos, boxRadius, i);
    }

    for (int k = 0; k < physicsIterations; ++k)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (!blocks[i]->IsActive() || blocks[i]->IsFilling() || blocks[i]->IsProjectile()) continue;

            auto* movA = blocks[i]->GetMovement();
            XMFLOAT3 posA = movA->GetPosition();
            float queryRadius = minTouchDist + 0.5f;
            auto nearbyBlockIndices = m_blockGrid.QueryRadius(posA, queryRadius);

            for (size_t j : nearbyBlockIndices)
            {
                if (j <= i) continue;
                if (!blocks[j]->IsActive() || blocks[j]->IsFilling() || blocks[j]->IsProjectile()) continue;

                auto* movB = blocks[j]->GetMovement();
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

                    bool a_anchor = blocks[i]->IsStacked();
                    bool b_anchor = blocks[j]->IsStacked();

                    if (a_anchor && !b_anchor) {
                        posB = movB->GetPosition();
                        if (useX) posB.x -= dir * moveAmt;
                        else posB.z -= dir * moveAmt;
                        movB->SetPosition(posB);
                    }
                    else if (!a_anchor && b_anchor) {
                        posA = movA->GetPosition();
                        if (useX) posA.x += dir * moveAmt;
                        else posA.z += dir * moveAmt;
                        movA->SetPosition(posA);
                    }
                    else {
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

void CollisionManager::CheckBlockVsEnemies()
{
    if (!m_blockManager || !m_enemyManager) return;
    if (m_blockManager->IsShieldActive()) return;

    auto& enemies = m_enemyManager->GetEnemies();
    auto& blocks = m_blockManager->GetBlocks();
    float blockRadius = 0.01f;
    const XMFLOAT3 PADDLE_EXTENTS = { 3.5f, 1.0f, 0.8f };
    const float BALL_RADIUS = 1.0f;

    for (auto it = enemies.begin(); it != enemies.end(); )
    {
        Enemy* enemy = it->get();
        if (!enemy) { ++it; continue; }

        XMFLOAT3 enemyPos = enemy->GetPosition();
        bool enemyDestroyed = false;
        float queryRadius = 5.0f;
        auto nearbyIndices = m_blockGrid.QueryRadius(enemyPos, queryRadius);

        for (size_t i : nearbyIndices)
        {
            if (i >= blocks.size()) continue;
            // Use pointer access
            Block& block = *blocks[i];

            if (!block.IsActive() || block.IsFalling()) continue;
            XMFLOAT3 blockPos = block.GetMovement()->GetPosition();

            float dx = blockPos.x - enemyPos.x;
            float dz = blockPos.z - enemyPos.z;
            float distSq = dx * dx + dz * dz;
            bool isHit = false;

            if (enemy->GetType() == EnemyType::Ball) {
                float rSum = blockRadius + BALL_RADIUS;
                if (distSq < rSum * rSum) isHit = true;
            }
            else {
                XMVECTOR vLocalPos = TransformToEnemyLocal(blockPos, enemy);
                XMFLOAT3 localPos;
                XMStoreFloat3(&localPos, vLocalPos);
                float closestX = (std::max)(-PADDLE_EXTENTS.x, (std::min)(localPos.x, PADDLE_EXTENTS.x));
                float closestZ = (std::max)(-PADDLE_EXTENTS.z, (std::min)(localPos.z, PADDLE_EXTENTS.z));
                float dX = localPos.x - closestX;
                float dZ = localPos.z - closestZ;
                if ((dX * dX + dZ * dZ) < (blockRadius * blockRadius)) isHit = true;
            }

            if (isHit)
            {
                if (m_blockManager && m_blockManager->IsInvincible()) {
                    enemyDestroyed = true;
                }
                else {
                    block.OnHit();
                    if (m_blockManager) m_blockManager->TriggerBlockBreakParams();
                    enemyDestroyed = true;
                }
                if (m_itemManager && enemy->GetType() == EnemyType::Paddle) {
                    m_itemManager->SpawnHealAt(enemyPos);
                }
                enemyDestroyed = true;
                break;
            }
        }

        if (enemyDestroyed) it = enemies.erase(it);
        else ++it;
    }
}

void CollisionManager::CheckBlockVsItems()
{
    if (!m_blockManager || !m_itemManager) return;

    auto& blocks = m_blockManager->GetBlocks();
    float bRadius = 0.5f;

    for (auto& item : m_itemManager->GetItems())
    {
        if (!item->IsActive()) continue;
        XMFLOAT3 iPos = item->GetPosition();
        float iRadius = item->scale.x * 0.5f;

        for (auto& blockPtr : blocks)
        {
            if (!blockPtr->IsActive()) continue;
            if (blockPtr->IsProjectile()) continue;

            XMFLOAT3 bPos = blockPtr->GetMovement()->GetPosition();
            float distSq = (bPos.x - iPos.x) * (bPos.x - iPos.x) +
                (bPos.z - iPos.z) * (bPos.z - iPos.z);

            float combinedRadius = bRadius + iRadius;

            if (distSq < combinedRadius * combinedRadius)
            {
                if (item->GetType() == ItemType::Heal) {
                    if (m_blockManager) m_blockManager->AddBlockFromItem(iPos);
                }
                else if (item->GetType() == ItemType::Invincible) {
                    if (m_player) m_player->ActivateInvincibility();
                    if (m_blockManager && m_player) {
                        m_blockManager->ActivateInvincibility(m_player->invincibleSettings.Duration);
                    }
                }
                item->SetActive(false);
                break;
            }
        }
    }
}

void CollisionManager::CheckPlayerVsBlocks()
{
    if (!m_player || !m_blockManager) return;
    if (m_blockManager->IsShieldActive()) return;
    XMFLOAT3 playerSize = { 0.4f, 0.4f, 0.4f };
    XMFLOAT3 blockSize = { 0.4f, 0.4f, 0.4f };
    CharacterMovement* playerMove = m_player->GetMovement();
    XMFLOAT3 playerPos = playerMove->GetPosition();
    XMFLOAT3 playerVel = playerMove->GetVelocity();

    for (int k = 0; k < 2; ++k)
    {
        for (const auto& blockPtr : m_blockManager->GetBlocks())
        {
            if (!blockPtr->IsActive() || blockPtr->IsFilling()) continue;
            if (blockPtr->IsProjectile()) continue;
            XMFLOAT3 blockPos = blockPtr->GetMovement()->GetPosition();
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

void CollisionManager::CheckPlayerVsEnemies()
{
    if (!m_player || !m_enemyManager) return;

    auto& enemies = m_enemyManager->GetEnemies();
    XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();
    float playerRadius = 0.01f;
    const XMFLOAT3 PADDLE_EXTENTS = { 3.5f, 1.0f, 0.8f };
    const float BALL_RADIUS = 1.0f;

    for (auto it = enemies.begin(); it != enemies.end(); )
    {
        Enemy* enemy = it->get();
        if (!enemy) { ++it; continue; }

        XMFLOAT3 enemyPos = enemy->GetPosition();
        bool isHit = false;
        float dx = playerPos.x - enemyPos.x;
        float dz = playerPos.z - enemyPos.z;
        float distSq = dx * dx + dz * dz;
        float maxReach = 6.0f;

        if (distSq > maxReach * maxReach) { ++it; continue; }

        if (enemy->GetType() == EnemyType::Ball) {
            float rSum = playerRadius + BALL_RADIUS;
            if (distSq < rSum * rSum) isHit = true;
        }
        else {
            XMVECTOR vLocalPos = TransformToEnemyLocal(playerPos, enemy);
            XMFLOAT3 localPos;
            XMStoreFloat3(&localPos, vLocalPos);
            float closestX = (std::max)(-PADDLE_EXTENTS.x, (std::min)(localPos.x, PADDLE_EXTENTS.x));
            float closestZ = (std::max)(-PADDLE_EXTENTS.z, (std::min)(localPos.z, PADDLE_EXTENTS.z));
            float dX = localPos.x - closestX;
            float dZ = localPos.z - closestZ;
            if ((dX * dX + dZ * dZ) < (playerRadius * playerRadius)) isHit = true;
        }

        if (isHit)
        {
            if (m_player && m_player->IsInvincible()) {
                // Player survives
            }
            else {
                if (m_onPlayerHitCallback) m_onPlayerHitCallback();
                m_player->SetInputEnabled(false);
                m_player->GetMovement()->SetPosition({ 0, -1000, 0 });
            }
            if (m_itemManager && enemy->GetType() == EnemyType::Paddle) {
                m_itemManager->SpawnHealAt(enemyPos);
            }
            it = enemies.erase(it);
        }
        else {
            ++it;
        }
    }
}

void CollisionManager::CheckBlockVsVoidLines()
{
    if (!m_blockManager || !m_stage) return;
    if (m_blockManager->IsShieldActive()) return;

    const float BLOCK_RADIUS = 0.5f;
    const float FALL_THRESHOLD = 0.1f;
    const float TRIGGER_RANGE = 2.0f;

    for (const auto& line : m_stage->m_linesVoid)
    {
        XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(line.Rotation.x),
            XMConvertToRadians(line.Rotation.y),
            XMConvertToRadians(line.Rotation.z)
        );
        XMMATRIX matInvRot = XMMatrixTranspose(matRot);
        float lineHalfLength = line.Scale.x * 0.5f;

        for (auto& blockPtr : m_blockManager->GetBlocks())
        {
            if (!blockPtr->IsActive() || blockPtr->IsFalling()) continue;
            if (blockPtr->IsProjectile()) continue;

            XMVECTOR vLocalPos = TransformToLocalLine(blockPtr->GetMovement()->GetPosition(), line);
            XMFLOAT3 localPos;
            XMStoreFloat3(&localPos, vLocalPos);

            if (localPos.x < -lineHalfLength - 0.5f || localPos.x > lineHalfLength + 0.5f) continue;

            XMVECTOR vWorldVel = XMLoadFloat3(&blockPtr->GetMovement()->GetVelocity());
            XMVECTOR vLocalVel = XMVector3TransformNormal(vWorldVel, matInvRot);
            float velZ = XMVectorGetZ(vLocalVel);

            if (velZ < 0.05f)
            {
                if (localPos.z < -FALL_THRESHOLD && localPos.z > -TRIGGER_RANGE)
                {
                    blockPtr->SetFalling(true);
                }
            }
        }
    }
}

void CollisionManager::CheckPlayerVsItems()
{
    if (!m_player || !m_itemManager) return;

    // Gunakan GetMovement()->GetPosition() agar konsisten dengan logika player lain
    XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
    float pRadius = 0.5f; // Radius player (sesuaikan jika perlu)

    for (auto& item : m_itemManager->GetItems())
    {
        if (!item->IsActive()) continue;

        XMFLOAT3 iPos = item->GetPosition();
        // Asumsi radius item dari scale
        float iRadius = item->scale.x * 0.5f;

        float distSq = (pPos.x - iPos.x) * (pPos.x - iPos.x) +
            (pPos.z - iPos.z) * (pPos.z - iPos.z);

        float combinedRadius = pRadius + iRadius;

        if (distSq < combinedRadius * combinedRadius)
        {
            // COLLISION TERJADI!

            // 1. Efek Item
            if (item->GetType() == ItemType::Heal)
            {
                if (m_blockManager) m_blockManager->AddBlockFromItem(iPos);
            }
            else if (item->GetType() == ItemType::Invincible)
            {
                if (m_player) m_player->ActivateInvincibility();
                if (m_blockManager && m_player) {
                    m_blockManager->ActivateInvincibility(m_player->invincibleSettings.Duration);
                }
            }

            // 2. Matikan Item
            item->SetActive(false);
        }
    }
}

// =========================================================
// [BARU] CHECK BOSS FILES VS PLAYER
// =========================================================
void CollisionManager::CheckBossFilesVsPlayer()
{
    if (!m_boss || !m_player) return;

    // Ambil referensi vector projectiles biar kodenya bersih
    auto& files = m_boss->GetProjectiles();

    // Ambil posisi & radius Player
    XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
    float pRadius = 0.5f; // Radius Player standar
    float fileRadius = 0.6f; // Radius File (agak besar biar gampang kena)

    for (auto& file : files)
    {
        if (!file.active) continue;

        // Hitung jarak squared (biar gak perlu akar kuadrat / sqrt)
        float dx = pPos.x - file.position.x;
        float dz = pPos.z - file.position.z; // Abaikan Y karena top-down logic
        float distSq = dx * dx + dz * dz;

        float combinedRadius = pRadius + fileRadius;

        if (distSq < (combinedRadius * combinedRadius))
        {
            // --- HIT PLAYER ---

            // 1. Matikan File
            file.active = false;

            // 2. Player Kena Hit (Logic sama seperti kena musuh/bola)
            if (m_player->IsInvincible())
            {
                // Kalau invincible, cuma file yang hancur, player aman
            }
            else
            {
                if (m_onPlayerHitCallback) m_onPlayerHitCallback();
                m_player->SetInputEnabled(false);
                m_player->GetMovement()->SetPosition({ 0, -1000, 0 }); // Lempar ke void (Game Over)
            }
        }
    }
}

// =========================================================
// [BARU] CHECK BOSS FILES VS BLOCKS
// =========================================================
void CollisionManager::CheckBossFilesVsBlocks()
{
    if (!m_boss || !m_blockManager) return;

    // Kita gunakan m_blockGrid yang sudah diisi oleh CheckBlockVsBlocks()
    // Ini jauh lebih cepat daripada loop 100 file x 64 block (6400 checks).

    auto& files = m_boss->GetProjectiles();
    auto& blocks = m_blockManager->GetBlocks();

    float fileRadius = 0.6f;
    float blockRadius = 0.4f; // Radius visual block

    for (auto& file : files)
    {
        if (!file.active) continue;

        // Query Grid: Cari block di sekitar posisi file
        float queryRadius = 2.0f;
        auto nearbyIndices = m_blockGrid.QueryRadius(file.position, queryRadius);

        for (size_t i : nearbyIndices)
        {
            if (i >= blocks.size()) continue;
            Block& block = *blocks[i];

            if (!block.IsActive() || block.IsFilling() || block.IsProjectile()) continue;

            XMFLOAT3 bPos = block.GetMovement()->GetPosition();

            // Cek Collision Sphere vs Sphere (Cepat & Efektif untuk proyektil banyak)
            float dx = bPos.x - file.position.x;
            float dz = bPos.z - file.position.z;
            float distSq = dx * dx + dz * dz;

            float combinedRadius = fileRadius + blockRadius;

            if (distSq < (combinedRadius * combinedRadius))
            {
                // --- HIT BLOCK ---

                // 1. Matikan File
                file.active = false;

                // 2. Hancurkan Block
                if (m_blockManager->IsInvincible())
                {
                    // Kalau mode invincible, block kuat (tidak hancur), cuma file yang hancur
                }
                else
                {
                    block.OnHit(); // Block hancur
                    m_blockManager->TriggerBlockBreakParams(); // Trigger callback/sound
                }

                // File sudah mati, break loop nearbyIndices agar tidak menghancurkan 2 block sekaligus
                break;
            }
        }
    }
}