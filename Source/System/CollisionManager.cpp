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
    CheckPlayerProjectilesVsEnemies();
    CheckPlayerVsEnemies();
    CheckPlayerVsCheckpointLines();
    CheckPlayerVsTriggerLines();
    CheckPlayerVsVoidLines();
    CheckBossFilesVsPlayer();

    if (m_itemManager)
    {
        CheckPlayerVsItems();
    }

    ProcessPlayerAttackContext();
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

            if (bullet->GetHomingTarget() != nullptr)
            {
                DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

                // Get the position of the TARGET, not the enemy who shot it!
                DirectX::XMFLOAT3 targetPos = bullet->GetHomingTarget()->GetPosition();

                float dx = bPos.x - targetPos.x;
                float dz = bPos.z - targetPos.z;
                float distSq = dx * dx + dz * dz;

                // If the bullet hits the target's radius (1.5f)
                if (distSq < (1.5f * 1.5f))
                {
                    // Safely cast the Character* back to an Enemy* to kill it
                    Enemy* targetEnemy = static_cast<Enemy*>(bullet->GetHomingTarget());
                    if (targetEnemy) {
                        targetEnemy->SetActive(false);
                    }

                    it = projectiles.erase(it);
                    continue;
                }
            }

            bool hitPlayer = false;

            if (m_player && bullet->GetHomingTarget() == nullptr)
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

    // Use references (&) to prevent copying heavy arrays into local memory
    auto& enemies = m_enemyManager->GetEnemies();
    DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();
    DirectX::XMFLOAT3 playerVel = m_player->GetMovement()->GetVelocity();

    constexpr float PLAYER_RADIUS = 0.25f;
    bool collidedAny = false;

    for (auto& enemy : enemies)
    {
        // EARLY EXIT: Skip empty pointers and dead enemies
        if (!enemy || !enemy->IsActive()) continue;

        DirectX::XMFLOAT3 ePos = enemy->GetPosition();

        // DYNAMIC HITBOXES
        // If you make an enemy 2x bigger, its physical wall becomes 2x bigger automatically.
        float enemyScale = enemy->GetScale().x;
        float enemyRadius = 0.1f * enemyScale;

        if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
        else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 0.8f * enemyScale;

        float combinedRadius = PLAYER_RADIUS + enemyRadius;

        float dx = playerPos.x - ePos.x;
        float dz = playerPos.z - ePos.z;
        float distSq = (dx * dx) + (dz * dz);

        // Check if player is penetrating the enemy's radius
        if (distSq < (combinedRadius * combinedRadius))
        {
            float dist = std::sqrt(distSq);

            // BUG PREVENTION: The Divide-By-Zero Guard
            if (dist < 0.0001f)
            {
                dx = 1.0f;
                dz = 0.0f;
                dist = 1.0f;
            }

            // Calculate exactly how deep the player is inside the enemy
            float overlap = combinedRadius - dist;

            // Push the player backward out of the enemy
            float pushX = (dx / dist) * overlap;
            float pushZ = (dz / dist) * overlap;

            playerPos.x += pushX;
            playerPos.z += pushZ;

            collidedAny = true;

            // BUG PREVENTION: The "Sticky Wall" Fix
            DirectX::XMVECTOR vVel = DirectX::XMLoadFloat3(&playerVel);
            DirectX::XMVECTOR vNormal = DirectX::XMVectorSet(dx / dist, 0.0f, dz / dist, 0.0f);

            float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(vVel, vNormal));
            if (dot < 0.0f)
            {
                vVel = DirectX::XMVectorSubtract(vVel, DirectX::XMVectorScale(vNormal, dot));
                DirectX::XMStoreFloat3(&playerVel, vVel);
            }
        }
    }

    // Only update the player's transform if a collision actually happened
    if (collidedAny)
    {
        m_player->SetPosition(playerPos);
        m_player->GetMovement()->SetVelocity(playerVel);
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

void CollisionManager::CheckPlayerProjectilesVsEnemies()
{
    if (!m_player || !m_enemyManager) return;

    auto& projectiles = m_player->GetProjectiles();
    auto& enemies = m_enemyManager->GetEnemies();

    constexpr int PLAYER_BULLET_DAMAGE = 10;
    constexpr float BULLET_HITBOX_RADIUS = 1.0f;

    for (auto& bullet : projectiles)
    {
        if (!bullet || !bullet->IsActive()) continue;

        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

        for (auto& enemy : enemies)
        {
            if (!enemy || !enemy->IsActive()) continue;

            DirectX::XMFLOAT3 ePos = enemy->GetPosition();

            if (CheckSphereCollision(bPos, ePos, BULLET_HITBOX_RADIUS))
            {
                enemy->TakeDamage(PLAYER_BULLET_DAMAGE);
                bullet->SetActive(false);

                break;
            }
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

bool CollisionManager::CheckSphereCollision(const DirectX::XMFLOAT3& posA, const DirectX::XMFLOAT3& posB, float threshold)
{
    float dx = posA.x - posB.x;
    float dz = posA.z - posB.z;
    float distSq = (dx * dx) + (dz * dz);
    float thresholdSq = threshold * threshold;

    return distSq < thresholdSq;
}

void CollisionManager::ProcessPlayerAttackContext()
{
    if (!m_player || !m_enemyManager) return;

    // Trigger only once per Spacebar press
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SPACE))
    {
        DirectX::XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();

        float parryThreshold = 2.5f;
        constexpr int PLAYER_SLASH_DAMAGE = 30;

        // Define the exact length of the sword blade past the player's body.
        // Tweak this number to make the attack feel perfectly close! (0.5f to 0.8f is usually best)
        constexpr float WEAPON_REACH = 0.8f;

		// 1. CONDITION: Distance(Enemy) < Threshold -> SLASH
        for (auto& enemy : m_enemyManager->GetEnemies())
        {
            if (!enemy || !enemy->IsActive()) continue;

            DirectX::XMFLOAT3 ePos = enemy->GetPosition();

            // ---> DYNAMIC HITBOX MATH <---
            // Get the visual edge of the enemy based on their specific type and scale
            float enemyScale = enemy->GetScale().x;
            float enemyRadius = 1.0f * enemyScale; // Default Ball

            if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
            else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 1.2f * enemyScale; // Tight radius for the thin paddle

            // The perfect "touching" distance: Player Body (0.5) + Enemy Body + Sword Length
            float exactSlashDistance = 0.5f + enemyRadius + WEAPON_REACH;

            if (CheckSphereCollision(pPos, ePos, exactSlashDistance))
            {
                // ---> FORCE THE PLAYER TO FACE THE ENEMY <---
                float dxToEnemy = ePos.x - pPos.x;
                float dzToEnemy = ePos.z - pPos.z;
                float distToEnemy = std::sqrt((dxToEnemy * dxToEnemy) + (dzToEnemy * dzToEnemy));

                if (distToEnemy > 0.001f)
                {
                    // Overwrite the WASD direction so the legs stay facing the enemy
                    DirectX::XMFLOAT2 slashDir = { dxToEnemy / distToEnemy, dzToEnemy / distToEnemy };
                    m_player->SetLastValidInput(slashDir);

                    // Instantly snap the 3D model's rotation
                    float angleDeg = DirectX::XMConvertToDegrees(atan2f(dxToEnemy, dzToEnemy));
                    m_player->GetMovement()->SetRotationY(angleDeg);

                    // Force the Torso to look at the enemy and lock it!
                    m_player->ForceAimTarget(ePos);
                    m_player->SetAimLocked(true);
                }
                // ------------------------------------------------

                m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerSlash>());

                // ---> APPLY DAMAGE <---
                enemy->TakeDamage(PLAYER_SLASH_DAMAGE);


                return; 
            }
        }

        // 2. CONDITION: Distance(Bullet) < Threshold -> PARRY
        for (auto& enemy : m_enemyManager->GetEnemies())
        {
            for (auto& bullet : enemy->GetProjectiles())
            {
                if (!bullet->IsActive()) continue;

                DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

                if (CheckSphereCollision(pPos, bPos, parryThreshold))
                {
                    // ---> FORCE THE PLAYER TO FACE THE BULLET <---
                    float dxToBullet = bPos.x - pPos.x;
                    float dzToBullet = bPos.z - pPos.z;

                    float distToBullet = std::sqrt((dxToBullet * dxToBullet) + (dzToBullet * dzToBullet));
                    if (distToBullet > 0.001f)
                    {
                        DirectX::XMFLOAT2 parryDir = { dxToBullet / distToBullet, dzToBullet / distToBullet };
                        m_player->SetLastValidInput(parryDir);

                        float angleDeg = DirectX::XMConvertToDegrees(atan2f(dxToBullet, dzToBullet));
                        m_player->GetMovement()->SetRotationY(angleDeg);

                        m_player->ForceAimTarget(bPos);
                        m_player->SetAimLocked(true);
                    }
                    m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerParry>());

                    // ---> FIND THE NEAREST ENEMY TO THE PLAYER <---
                    Enemy* nearestEnemy = nullptr;
                    float closestDistSq = 9999999.0f;

                    for (auto& potentialTarget : m_enemyManager->GetEnemies())
                    {
                        if (!potentialTarget->IsActive()) continue;

                        DirectX::XMFLOAT3 ePos = potentialTarget->GetPosition();
                        float dxTarget = pPos.x - ePos.x;
                        float dzTarget = pPos.z - ePos.z;
                        float targetDistSq = (dxTarget * dxTarget) + (dzTarget * dzTarget);

                        if (targetDistSq < closestDistSq)
                        {
                            closestDistSq = targetDistSq;
                            nearestEnemy = potentialTarget.get();
                        }
                    }

                    if (!nearestEnemy) nearestEnemy = enemy.get();

                    bullet->SetHomingTarget(nearestEnemy);

                    // ---> INITIAL DEFLECTION <---
                    DirectX::XMFLOAT3 targetPos = nearestEnemy->GetPosition();
                    float defX = targetPos.x - bPos.x;
                    float defZ = targetPos.z - bPos.z;
                    float defDist = std::sqrt((defX * defX) + (defZ * defZ));

                    DirectX::XMFLOAT3 deflectDir = { 0.0f, 0.0f, 1.0f };
                    if (defDist > 0.001f) {
                        deflectDir = { defX / defDist, 0.0f, defZ / defDist };
                    }

                    DirectX::XMFLOAT3 currentVel = bullet->GetVelocity();
                    DirectX::XMVECTOR vCurrentVel = DirectX::XMLoadFloat3(&currentVel);
                    float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(vCurrentVel)) * 2.5f;

                    DirectX::XMVECTOR vNewVel = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&deflectDir), speed);
                    DirectX::XMFLOAT3 homingVel;
                    DirectX::XMStoreFloat3(&homingVel, vNewVel);

                    bullet->ApplyMovement(bPos, homingVel);

                    return;
                }
            }
        }

        // CONDITION: Else -> SHOOT
        m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerShoot>());
        m_player->FireProjectile();
    }
}