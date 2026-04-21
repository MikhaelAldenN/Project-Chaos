#include "CollisionManager.h"

using namespace DirectX;

// =========================================================
// HELPER FUNCTIONS 
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

// =========================================================
// INITIALIZATION OVERLOADS
// =========================================================

void CollisionManager::Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im)
{
    m_player = p;
    m_stage = s;
    m_enemyManager = em;
    m_itemManager = im;
    m_boss = nullptr;
}

void CollisionManager::Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im, Boss* boss)
{
    m_player = p;
    m_stage = s;
    m_enemyManager = em;
    m_itemManager = im;
    m_boss = boss;
}

// =========================================================
// UPDATE
// =========================================================

void CollisionManager::Update(float elapsedTime)
{
    CheckEnemyProjectilesFull(elapsedTime);
    CheckStageCollision();
    CheckPlayerVsEnemies();
    CheckPlayerVsCheckpointLines();
    CheckPlayerVsTriggerLines();
    CheckPlayerVsVoidLines();
    CheckBossFilesVsPlayer();

    if (m_itemManager)
    {
        CheckPlayerVsItems();
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
            auto& bullet = *it;
            if (!bullet) { ++it; continue; }

            XMFLOAT3 currentPos = bullet->GetMovement()->GetPosition();
            XMFLOAT3 currentVel = bullet->GetVelocity();
            XMVECTOR vPos = XMLoadFloat3(&currentPos);
            XMVECTOR vVel = XMLoadFloat3(&currentVel);
            XMVECTOR vNextPos = vPos + (vVel * elapsedTime);
            XMVECTOR vDir = XMVector3Normalize(vVel);

            float speed = XMVectorGetX(XMVector3Length(vVel));
            float frameDist = speed * elapsedTime;
            float bulletRadius = bullet->GetRadius();

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
                    float t = RayCastOBB(vPos, vDir, frameDist, bulletRadius, wall, tempNormal);

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
                finalPos.y = 0.0f; finalVel.y = 0.0f;

                bullet->ApplyMovement(finalPos, finalVel);
                ++it;
                continue;
            }

            bool hitPlayer = false;

            if (m_player)
            {
                DirectX::XMFLOAT3 bulletPos = bullet->GetMovement()->GetPosition();

                float playerHalfSize = 0.3f;
                float maxRange = playerHalfSize + bulletRadius;

                DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();

                float dx = bulletPos.x - playerPos.x;
                float dz = bulletPos.z - playerPos.z;
                float distSq = dx * dx + dz * dz;
                float searchRangeSq = (maxRange * 2.0f) * (maxRange * 2.0f);

                if (distSq <= searchRangeSq)
                {
                    float absX = std::abs(dx);
                    float absZ = std::abs(dz);

                    if (absX < maxRange && absZ < maxRange)
                    {
                        if (m_onPlayerDeathCallback) m_onPlayerDeathCallback();

                        it = projectiles.erase(it);
                        continue;
                    }
                }
            }

            XMFLOAT3 nextPosFloat;
            XMStoreFloat3(&nextPosFloat, vNextPos);
            bullet->ApplyMovement(nextPosFloat, currentVel);

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
            if (m_onPlayerDeathCallback) m_onPlayerDeathCallback();
            it = enemies.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckPlayerVsCheckpointLines()
{
    //if (!m_player || !m_stage) return;
    //if (m_player->IsFalling()) return;

    //const float TRIGGER_RANGE_Z = 2.0f;

    //for (const auto& line : m_stage->m_linesCheckpoint)
    //{
    //    XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
    //    XMFLOAT3 localPos;
    //    XMStoreFloat3(&localPos, vLocalPos);
    //    float lineHalfLength = line.Scale.x * 0.5f;

    //    if (localPos.x < -lineHalfLength || localPos.x > lineHalfLength) continue;
    //    if (localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
    //    {
    //        if (m_onCheckpointReachCallback)
    //        {
    //            m_onCheckpointReachCallback(line.Position);
    //        }
    //    }
    //}
}

void CollisionManager::CheckPlayerVsTriggerLines()
{
    //if (!m_player || !m_stage) return;
    //if (m_player->IsFalling()) return;

    //const float TRIGGER_RANGE_Z = 1.0f;

    //for (const auto& line : m_stage->m_linesDisable)
    //{
    //    XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
    //    XMFLOAT3 localPos;
    //    XMStoreFloat3(&localPos, vLocalPos);
    //    float lineHalfLength = line.Scale.x * 0.5f;

    //    if (localPos.x >= -lineHalfLength && localPos.x <= lineHalfLength &&
    //        localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
    //    {
    //        m_player->SetAbilityShield(false);
    //    }
    //}

    //for (int i = 0; i < m_stage->m_linesEnable.size(); ++i)
    //{
    //    const auto& line = m_stage->m_linesEnable[i];

    //    XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
    //    XMFLOAT3 localPos;
    //    XMStoreFloat3(&localPos, vLocalPos);
    //    float lineHalfLength = line.Scale.x * 0.5f;

    //    if (localPos.x >= -lineHalfLength && localPos.x <= lineHalfLength &&
    //        localPos.z > -TRIGGER_RANGE_Z && localPos.z < TRIGGER_RANGE_Z)
    //    {
    //        if (i == 0)
    //        {
    //            m_player->SetAbilityShield(true);
    //        }
    //        else if (i == 1)
    //        {
    //            m_player->SetAbilityShield(true);
    //            m_player->SetAbilityShoot(true);
    //        }
    //        else if (i == 2)
    //        {
    //            m_player->SetAbilityShield(true);
    //            if (m_onLevelCompleteCallback) m_onLevelCompleteCallback();
    //        }
    //    }
    //}
}

void CollisionManager::CheckPlayerVsVoidLines()
{
    //if (!m_player || !m_stage) return;
    //if (m_player->IsFalling()) return;

    //const float FALL_THRESHOLD = 0.1f;
    //const float TRIGGER_RANGE = 2.0f;

    //for (const auto& line : m_stage->m_linesVoid)
    //{
    //    XMVECTOR vLocalPos = TransformToLocalLine(m_player->GetMovement()->GetPosition(), line);
    //    XMFLOAT3 localPos;
    //    XMStoreFloat3(&localPos, vLocalPos);
    //    float lineHalfLength = line.Scale.x * 0.5f;

    //    if (localPos.x < -lineHalfLength - 0.5f || localPos.x > lineHalfLength + 0.5f) continue;
    //    if (localPos.z < -FALL_THRESHOLD && localPos.z > -TRIGGER_RANGE)
    //    {
    //        m_player->SetFalling(true);
    //    }
    //}
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
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Pop.wav", 0.4f);

            if (item->GetType() == ItemType::Invincible)
            {
            }

            item->SetActive(false);
        }
    }
}

void CollisionManager::CheckBossFilesVsPlayer()
{
    if (!m_boss || !m_player) return;

    auto& files = m_boss->GetProjectiles();

    XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
    float pRadius = 0.5f;
    float fileRadius = 0.6f;

    for (auto& file : files)
    {
        if (!file.active) continue;

        float dx = pPos.x - file.position.x;
        float dz = pPos.z - file.position.z;
        float distSq = dx * dx + dz * dz;

        float combinedRadius = pRadius + fileRadius;

        if (distSq < (combinedRadius * combinedRadius))
        {
            file.active = false;
            if (m_onPlayerHitCallback) m_onPlayerHitCallback();
            m_player->SetInputEnabled(false);
            m_player->GetMovement()->SetPosition({ 0, -1000, 0 });
        }
    }
}