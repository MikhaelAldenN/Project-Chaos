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

static float RayCastOBB(XMVECTOR rayOrigin, XMVECTOR rayDir, float rayLength, float radius, const DebugWallData& wall, XMVECTOR& outNormal)
{
    XMVECTOR vWallPos = XMLoadFloat3(&wall.Position);
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );
    XMMATRIX matInvRot = XMMatrixTranspose(matRot); 

    XMVECTOR vRelOrigin = XMVectorSubtract(rayOrigin, vWallPos);
    XMVECTOR vLocalOrigin = XMVector3TransformNormal(vRelOrigin, matInvRot);
    XMVECTOR vLocalDir = XMVector3TransformNormal(rayDir, matInvRot);

    float r = radius;
    float minX = -wall.Scale.x - r; float maxX = wall.Scale.x + r;
    float minZ = -wall.Scale.z - r; float maxZ = wall.Scale.z + r;
    float minY = -100.0f;           float maxY = 100.0f; 

    float tMin = 0.0f;
    float tMax = rayLength;

    XMFLOAT3 start, dir;
    XMStoreFloat3(&start, vLocalOrigin);
    XMStoreFloat3(&dir, vLocalDir);

    if (abs(dir.x) < 1e-6f) {
        if (start.x < minX || start.x > maxX) return -1.0f;
    }
    else {
        float invD = 1.0f / dir.x;
        float t1 = (minX - start.x) * invD;
        float t2 = (maxX - start.x) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = (std::max)(tMin, t1);
        tMax = (std::min)(tMax, t2);
        if (tMin > tMax) return -1.0f;
    }

    if (abs(dir.z) < 1e-6f) {
        if (start.z < minZ || start.z > maxZ) return -1.0f;
    }
    else {
        float invD = 1.0f / dir.z;
        float t1 = (minZ - start.z) * invD;
        float t2 = (maxZ - start.z) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = (std::max)(tMin, t1);
        tMax = (std::min)(tMax, t2);
        if (tMin > tMax) return -1.0f;
    }

    XMFLOAT3 hitPoint;
    XMStoreFloat3(&hitPoint, vLocalOrigin + vLocalDir * tMin);

    float distMinX = abs(hitPoint.x - minX);
    float distMaxX = abs(hitPoint.x - maxX);
    float distMinZ = abs(hitPoint.z - minZ);
    float distMaxZ = abs(hitPoint.z - maxZ);

    float bestDist = distMinX;
    XMVECTOR localNormal = XMVectorSet(-1, 0, 0, 0);

    if (distMaxX < bestDist) { bestDist = distMaxX; localNormal = XMVectorSet(1, 0, 0, 0); }
    if (distMinZ < bestDist) { bestDist = distMinZ; localNormal = XMVectorSet(0, 0, -1, 0); }
    if (distMaxZ < bestDist) { bestDist = distMaxZ; localNormal = XMVectorSet(0, 0, 1, 0); }

    outNormal = XMVector3TransformNormal(localNormal, matRot);

    return tMin;
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
    if (!m_player->IsFalling())
    {
        m_player->GetMovement()->SetGravityEnabled(false);
        m_player->GetMovement()->SetVelocityY(0.0f);
    }

    else
    {
        float currentY = m_player->GetMovement()->GetPosition().y;
        float limitY = Player::MovementSettings::KillPlaneY;

        if (currentY < limitY)
        {
            if (m_onPlayerDeathCallback) { m_onPlayerDeathCallback(); }
            m_player->SetFalling(false);
            m_player->GetMovement()->SetVelocity({ 0, 0, 0 });
            m_player->GetMovement()->SetGravityEnabled(false);
        }
    }

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
    CheckPlayerVsCheckpointLines();
    CheckPlayerVsTriggerLines();
    CheckPlayerVsVoidLines();
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

    for (auto& enemy : m_enemyManager->GetEnemies())
    {
        auto& projectiles = enemy->GetProjectiles();
        AttackType type = enemy->GetAttackType();

        for (auto it = projectiles.begin(); it != projectiles.end(); )
        {
            auto& ball = *it;
            if (!ball) { ++it; continue; }

            XMFLOAT3 currentPos = ball->GetMovement()->GetPosition();
            XMFLOAT3 currentVel = ball->GetVelocity();

            XMVECTOR vPos = XMLoadFloat3(&currentPos);
            XMVECTOR vVel = XMLoadFloat3(&currentVel);
            XMVECTOR vNextPos = vPos + (vVel * elapsedTime);
            XMVECTOR vDir = XMVector3Normalize(vVel);

            float speed = XMVectorGetX(XMVector3Length(vVel));
            float frameDist = speed * elapsedTime;
            float ballRadius = ball->GetRadius();

            bool hitWall = false;
            float closestT = frameDist;
            XMVECTOR hitNormal = XMVectorZero();

            if (m_stage)
            {
                for (const auto& wall : m_stage->m_debugWalls)
                {
                    float dx = currentPos.x - wall.Position.x;
                    float dz = currentPos.z - wall.Position.z;

                    float wallMax = (std::max)(wall.Scale.x, wall.Scale.z);
                    if ((dx * dx + dz * dz) > pow(wallMax + frameDist + 10.0f, 2)) continue;

                    XMVECTOR tempNormal;
                    float t = RayCastOBB(vPos, vDir, frameDist, ballRadius, wall, tempNormal);

                    if (t >= 0.0f && t < closestT)
                    {
                        closestT = t;
                        hitNormal = tempNormal;
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

                float safeDist = (std::max)(0.0f, closestT - 0.01f);
                XMVECTOR vSafePos = vPos + (vDir * safeDist);

                XMVECTOR vReflectedVel = XMVector3Reflect(vVel, hitNormal);

                float remainingDist = frameDist - closestT;
                vSafePos += (XMVector3Normalize(vReflectedVel) * remainingDist);

                vSafePos += hitNormal * 0.05f;

                XMFLOAT3 finalPos, finalVel;
                XMStoreFloat3(&finalPos, vSafePos);
                XMStoreFloat3(&finalVel, vReflectedVel);

                finalPos.y = 0.0f;
                finalVel.y = 0.0f;

                ball->ApplyMovement(finalPos, finalVel);
                ball->UpdatePreviousPosition(); 

                ++it;
                continue;
            }

            XMFLOAT3 nextPosFloat;
            XMStoreFloat3(&nextPosFloat, vNextPos);
            ball->ApplyMovement(nextPosFloat, currentVel);
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
            float maxScale = (std::max)(wall.Scale.x, wall.Scale.z);
            float dx = playerPos.x - wall.Position.x;
            float dz = playerPos.z - wall.Position.z;
            float distSq = (dx * dx) + (dz * dz);
            float checkDist = maxScale + playerRadius + 2.0f;
            if (distSq > (checkDist * checkDist)) continue;

            XMVECTOR vLocalPos = TransformToLocal(playerPos, wall);
            XMFLOAT3 localPos;
            XMStoreFloat3(&localPos, vLocalPos);

            float closestX = (std::max)(-wall.Scale.x, (std::min)(localPos.x, wall.Scale.x));
            float closestZ = (std::max)(-wall.Scale.z, (std::min)(localPos.z, wall.Scale.z));

            float localDx = localPos.x - closestX;
            float localDz = localPos.z - closestZ;
            float localDistSq = (localDx * localDx) + (localDz * localDz);

            if (localDistSq < (playerRadius * playerRadius) && localDistSq > 0.00001f)
            {
                float localDist = sqrt(localDistSq);
                float penetrationDepth = playerRadius - localDist;

                XMVECTOR vLocalNormal = XMVectorSet(localDx / localDist, 0.0f, localDz / localDist, 0.0f);

                XMVECTOR vWorldNormal = TransformNormalToWorld(vLocalNormal, wall);

                XMVECTOR vPush = XMVectorScale(vWorldNormal, penetrationDepth);
                XMVECTOR vCurrentPos = XMLoadFloat3(&playerPos);
                vCurrentPos = XMVectorAdd(vCurrentPos, vPush);
                XMStoreFloat3(&playerPos, vCurrentPos);

                collidedAny = true;

                XMVECTOR vVel = XMLoadFloat3(&vel);
                float dot = XMVectorGetX(XMVector3Dot(vVel, vWorldNormal));
                if (dot < 0.0f) {
                    vVel = XMVectorSubtract(vVel, XMVectorScale(vWorldNormal, dot));
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
        Block& block = *blockPtr;

        if (!block.IsActive()) continue;
        block.ResetCollisionState();

        auto* moveComp = block.GetMovement();

        if (block.IsProjectile())
        {
            XMFLOAT3 blockPos = moveComp->GetPosition();

            float queryRadius = blockRadius + 5.0f;
            auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(blockPos, queryRadius);

            for (size_t wallIdx : nearbyWallIndices)
            {
                const auto& wall = m_stage->m_debugWalls[wallIdx];

                float maxScale = (std::max)(wall.Scale.x, wall.Scale.z);
                float dx = blockPos.x - wall.Position.x;
                float dz = blockPos.z - wall.Position.z;
                if ((dx * dx + dz * dz) > pow(maxScale + blockRadius + 2.0f, 2)) continue;

                XMVECTOR vLocalPos = TransformToLocal(blockPos, wall);
                XMFLOAT3 localPos;
                XMStoreFloat3(&localPos, vLocalPos);

                float closestX = (std::max)(-wall.Scale.x, (std::min)(localPos.x, wall.Scale.x));
                float closestZ = (std::max)(-wall.Scale.z, (std::min)(localPos.z, wall.Scale.z));
                float localDx = localPos.x - closestX;
                float localDz = localPos.z - closestZ;
                float localDistSq = (localDx * localDx) + (localDz * localDz);

                if (localDistSq < (blockRadius * blockRadius) && localDistSq > 0.00001f)
                {
                    block.OnHit();
                    if (m_blockManager) m_blockManager->TriggerBlockBreakParams();

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

            float queryRadius = blockRadius + 5.0f;
            auto nearbyWallIndices = m_stage->GetSpatialGrid().QueryRadius(blockPos, queryRadius);

            for (size_t wallIdx : nearbyWallIndices)
            {
                const auto& wall = m_stage->m_debugWalls[wallIdx];

                float maxScale = (std::max)(wall.Scale.x, wall.Scale.z);
                float dx = blockPos.x - wall.Position.x;
                float dz = blockPos.z - wall.Position.z;
                if ((dx * dx + dz * dz) > pow(maxScale + blockRadius + 2.0f, 2)) continue;

                XMVECTOR vLocalPos = TransformToLocal(blockPos, wall);
                XMFLOAT3 localPos;
                XMStoreFloat3(&localPos, vLocalPos);

                float closestX = (std::max)(-wall.Scale.x, (std::min)(localPos.x, wall.Scale.x));
                float closestZ = (std::max)(-wall.Scale.z, (std::min)(localPos.z, wall.Scale.z));

                float localDx = localPos.x - closestX;
                float localDz = localPos.z - closestZ;
                float localDistSq = (localDx * localDx) + (localDz * localDz);

                if (localDistSq < (blockRadius * blockRadius) && localDistSq > 0.00001f)
                {
                    float localDist = sqrt(localDistSq);
                    float penetration = blockRadius - localDist;

                    XMVECTOR vLocalNormal = XMVectorSet(localDx / localDist, 0.0f, localDz / localDist, 0.0f);
                    XMVECTOR vWorldNormal = TransformNormalToWorld(vLocalNormal, wall);

                    XMVECTOR vPush = XMVectorScale(vWorldNormal, penetration);

                    XMVECTOR vPos = XMLoadFloat3(&blockPos);
                    vPos = XMVectorAdd(vPos, vPush);
                    XMStoreFloat3(&blockPos, vPos);

                    collidedAny = true;
                    block.SetWallCollision({ 0, 1, 0 });

                    XMVECTOR vVel = XMLoadFloat3(&vel);
                    float dot = XMVectorGetX(XMVector3Dot(vVel, vWorldNormal));
                    if (dot < 0.0f)
                    {
                        vVel = XMVectorSubtract(vVel, XMVectorScale(vWorldNormal, dot));
                        XMStoreFloat3(&vel, vVel);
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

    float boxRadius = 0.4f;
    auto& blocks = m_blockManager->GetBlocks();
    size_t count = blocks.size();

    m_blockGrid.Clear();

    for (size_t i = 0; i < count; ++i)
    {
        // Add all active blocks to the grid
        if (!blocks[i]->IsActive() || blocks[i]->IsFilling()) continue;
        XMFLOAT3 pos = blocks[i]->GetMovement()->GetPosition();
        m_blockGrid.Insert(pos, boxRadius, i);
    }

    if (m_blockManager->IsShieldActive()) return;

    float minTouchDist = boxRadius * 2.45f;
    float pushTarget = minTouchDist + 0.001f;
    int physicsIterations = 4;

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

    auto& enemies = m_enemyManager->GetEnemies();
    auto& blocks = m_blockManager->GetBlocks();

    float blockRadius = 0.5f;
    const float BALL_RADIUS = 0.1f;     
    const float PADDLE_THICKNESS = 0.5f;
    const float PADDLE_WIDTH_HALF = 0.8f; 

    for (auto it = enemies.begin(); it != enemies.end(); )
    {
        Enemy* enemy = it->get();
        if (!enemy) { ++it; continue; }

        XMFLOAT3 enemyPos = enemy->GetPosition();
        bool enemyDestroyed = false;

        float queryRadius = 7.0f;
        auto nearbyIndices = m_blockGrid.QueryRadius(enemyPos, queryRadius);

        for (size_t i : nearbyIndices)
        {
            if (i >= blocks.size()) continue;
            Block& block = *blocks[i];

            if (!block.IsActive() || block.IsFalling()) continue;

            XMFLOAT3 blockPos = block.GetMovement()->GetPosition();
            XMVECTOR vBlockLocal = TransformToEnemyLocal(blockPos, enemy);
            XMFLOAT3 localPos;
            XMStoreFloat3(&localPos, vBlockLocal);

            float distSq = 0.0f;
            float combinedRadius = 0.0f;

            if (enemy->GetType() == EnemyType::Ball)
            {
                distSq = localPos.x * localPos.x + localPos.z * localPos.z;
                combinedRadius = blockRadius + BALL_RADIUS;
            }
            else 
            {
                float closestX = (std::max)(-PADDLE_WIDTH_HALF, (std::min)(localPos.x, PADDLE_WIDTH_HALF));
                float dx = localPos.x - closestX;
                float dz = localPos.z - 0.0f; 

                distSq = dx * dx + dz * dz;
                combinedRadius = blockRadius + PADDLE_THICKNESS;
            }

            if (distSq < (combinedRadius * combinedRadius))
            {
                block.OnHit();

                if (m_blockManager) m_blockManager->TriggerBlockBreakParams();
                if (m_itemManager && enemy->GetType() == EnemyType::Paddle)
                {
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
                    float duration = Player::InvincibleSettings::Duration;
                    if (m_blockManager) m_blockManager->ActivateInvincibility(duration);
                    if (m_player) m_player->ActivateInvincibility(duration);
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

    float playerRadius = 0.5f;
    const float BALL_RADIUS = 1.0f;
    const float PADDLE_THICKNESS = 0.5f;
    const float PADDLE_WIDTH_HALF = 0.8f;

    for (auto it = enemies.begin(); it != enemies.end(); )
    {
        Enemy* enemy = it->get();
        if (!enemy) { ++it; continue; }

        XMFLOAT3 enemyPos = enemy->GetPosition();
        XMVECTOR vPlayerLocal = TransformToEnemyLocal(playerPos, enemy);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, vPlayerLocal);

        float distSq = 0.0f;
        float combinedRadius = 0.0f;

        if (enemy->GetType() == EnemyType::Ball)
        {
            distSq = localPos.x * localPos.x + localPos.z * localPos.z;
            combinedRadius = playerRadius + BALL_RADIUS;
        }
        else 
        {
            float closestX = (std::max)(-PADDLE_WIDTH_HALF, (std::min)(localPos.x, PADDLE_WIDTH_HALF));
            float dx = localPos.x - closestX;
            float dz = localPos.z - 0.0f;

            distSq = dx * dx + dz * dz;
            combinedRadius = playerRadius + PADDLE_THICKNESS;
        }

        if (distSq < (combinedRadius * combinedRadius))
        {
            if (m_player->IsInvincible())
            {
                if (m_itemManager && enemy->GetType() == EnemyType::Paddle) {
                    m_itemManager->SpawnHealAt(enemyPos);
                }
                it = enemies.erase(it);
                continue;
            }

            if (m_onPlayerHitCallback) m_onPlayerHitCallback();

            if (m_itemManager && enemy->GetType() == EnemyType::Paddle) {
                m_itemManager->SpawnHealAt(enemyPos);
            }
            it = enemies.erase(it);

            m_player->SetInputEnabled(false);
            m_player->GetMovement()->SetPosition({ 0, -1000, 0 });
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckPlayerVsCheckpointLines()
{
    if (!m_player || !m_stage) return;
    if (m_player->IsFalling()) return;

    const float TRIGGER_RANGE_Z = 2.0f;

    for (const auto& line : m_stage->m_linesCheckpoint)
    {
        XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, vLocalPos);
        float lineHalfLength = line.Scale.x * 0.5f;

        if (localPos.x < -lineHalfLength || localPos.x > lineHalfLength) continue;
        if (localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
        {
            if (m_onCheckpointReachCallback)
            {
                m_onCheckpointReachCallback(line.Position);
            }
        }
    }
}

void CollisionManager::CheckPlayerVsTriggerLines()
{
    if (!m_player || !m_stage) return;
    if (m_player->IsFalling()) return;

    const float TRIGGER_RANGE_Z = 1.0f;

    for (const auto& line : m_stage->m_linesDisable)
    {
        XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, vLocalPos);
        float lineHalfLength = line.Scale.x * 0.5f;

        if (localPos.x >= -lineHalfLength && localPos.x <= lineHalfLength &&
            localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
        {
            m_player->SetAbilityShield(false);
        }
    }

    for (int i = 0; i < m_stage->m_linesEnable.size(); ++i)
    {
        const auto& line = m_stage->m_linesEnable[i];

        XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, vLocalPos);
        float lineHalfLength = line.Scale.x * 0.5f;

        if (localPos.x >= -lineHalfLength && localPos.x <= lineHalfLength &&
            localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
        {
            if (i == 0)
            {
                m_player->SetAbilityShield(true);
            }
            else if (i == 1)
            {
                m_player->SetAbilityShield(true);
                m_player->SetAbilityShoot(true);
            }
            else if (i == 2)
            {
                m_player->SetAbilityShield(true);
            }
        }
    }
}

void CollisionManager::CheckPlayerVsVoidLines()
{
    if (!m_player || !m_stage) return;
    if (m_player->IsFalling()) return;

    const float FALL_THRESHOLD = 0.1f;
    const float TRIGGER_RANGE = 2.0f;

    for (const auto& line : m_stage->m_linesVoid)
    {
        XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, vLocalPos);
        float lineHalfLength = line.Scale.x * 0.5f;

        if (localPos.x < -lineHalfLength - 0.5f || localPos.x > lineHalfLength + 0.5f) continue;
        if (localPos.z < -FALL_THRESHOLD && localPos.z > -TRIGGER_RANGE)
        {
            m_player->SetFalling(true);
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

    XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
    float pRadius = 0.5f;

    for (auto& item : m_itemManager->GetItems())
    {
        if (!item->IsActive()) continue;

        XMFLOAT3 iPos = item->GetPosition();
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
            // Handle Invincible Item
            else if (item->GetType() == ItemType::Invincible)
            {
                float duration = Player::InvincibleSettings::Duration;

                // Apply to Blocks (The Shield)
                if (m_blockManager) m_blockManager->ActivateInvincibility(duration);

                // Apply to Player (The Center Character)
                if (m_player) m_player->ActivateInvincibility(duration);
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