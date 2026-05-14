#include "CollisionManager.h"
#include "NaviBoss.h"        
#include "NaviPhaseNormal.h" 
#include "TimeManager.h"
#include <CameraController.h>
#include "NaviPhaseWindowkill.h"

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

static float DistancePointToLineSegment2D(const DirectX::XMFLOAT3& A, const DirectX::XMFLOAT3& B, const DirectX::XMFLOAT3& P)
{
    float lineX = B.x - A.x;
    float lineZ = B.z - A.z;
    float px = P.x - A.x;
    float pz = P.z - A.z;

    float lineLengthSq = (lineX * lineX) + (lineZ * lineZ);

    // If the bullet didn't move this frame, just do a normal sphere check
    if (lineLengthSq == 0.0f) return std::sqrt((px * px) + (pz * pz));

    // Dot product to project the target's position onto the bullet's path line
    float t = ((px * lineX) + (pz * lineZ)) / lineLengthSq;

    // Clamp 't' between 0.0f and 1.0f 
    t = (std::max)(0.0f, (std::min)(1.0f, t));

    // Find the closest point on the line
    float closestX = A.x + t * lineX;
    float closestZ = A.z + t * lineZ;

    float dx = P.x - closestX;
    float dz = P.z - closestZ;

    return std::sqrt((dx * dx) + (dz * dz));
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
    
    CheckNaviProjectilesVsEnemies(elapsedTime);
    CheckNaviBossProjectilesVsPlayer(elapsedTime);
    CheckNaviBossProjectilesVsBoss(elapsedTime);

    // =========================================================
    // DETEKSI PELURU PLAYER VS WINDOW BOSS (AABB COLLISION)
    // =========================================================
    if (m_naviBoss && m_player) {
        if (auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_naviBoss->GetCurrentPhase())) {

            if (!normalPhase->IsDead()) {
                DirectX::XMFLOAT3 bossPos = m_naviBoss->GetPosition();

                // Ukuran Window di 3D World adalah 5.0f (Radius/Setengahnya adalah 2.5f)
                float halfW = 2.5f;
                float halfD = 2.5f;

                // Asumsi: m_player memiliki fungsi GetProjectiles() yang me-return peluru player
                for (auto& bullet : m_player->GetProjectiles()) {
                    if (!bullet->IsActive()) continue;

                    DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

                    // Pengecekan Kotak (AABB): Apakah titik peluru berada di DALAM kotak Window?
                    if (bPos.x > (bossPos.x - halfW) && bPos.x < (bossPos.x + halfW) &&
                        bPos.z >(bossPos.z - halfD) && bPos.z < (bossPos.z + halfD))
                    {
                        // BOOM! Kena kaca window!
                        bullet->SetActive(false); // Hancurkan peluru player
                        normalPhase->TakeDamage(10); // Asumsi 1 peluru = 10 Damage

                        // Opsional: Mainkan suara kaca retak / benturan peluru di sini
                        // AudioManager::Instance().PlaySFX("Hit.wav");
                    }
                }
            }
        }
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

            if (bullet->GetHomingTarget() != nullptr)
            {
                Enemy* targetEnemy = static_cast<Enemy*>(bullet->GetHomingTarget());

                // ---> BUG PREVENTION: The Dangling Pointer Guard <---
                // Verify the target wasn't deleted from memory by a different attack (like a Slash)
                bool isTargetAlive = false;
                for (auto& activeEnemy : m_enemyManager->GetEnemies()) {
                    if (activeEnemy.get() == targetEnemy && activeEnemy->IsActive()) {
                        isTargetAlive = true;
                        break;
                    }
                }

                // If the target died while the bullet was mid-air, destroy the bullet to prevent a crash.
                if (!isTargetAlive) {
                    it = projectiles.erase(it);
                    continue;
                }

                DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
                DirectX::XMFLOAT3 targetPos = targetEnemy->GetPosition();

                float dx = bPos.x - targetPos.x;
                float dz = bPos.z - targetPos.z;
                float distSq = dx * dx + dz * dz;

                // ---> DYNAMIC HITBOX MATH <---
                float enemyScale = targetEnemy->GetScale().x;
                float hitRadius = 1.0f * enemyScale;

                if (targetEnemy->GetType() == EnemyType::Pentagon) hitRadius = 4.0f * enemyScale;
                else if (targetEnemy->GetType() == EnemyType::Paddle) hitRadius = 0.8f * enemyScale;

                float combinedHitRadius = hitRadius + bullet->GetRadius();

                if (distSq < (combinedHitRadius * combinedHitRadius))
                {
                    // ---> APPLY PARRY DAMAGE <---
                    constexpr int PARRY_DAMAGE = 30;
                    targetEnemy->TakeDamage(PARRY_DAMAGE);

                    // Destroy the bullet
                    it = projectiles.erase(it);
                    continue;
                }
            }

            bool hitPlayer = false;

            // ----------------------------------------------------
            // ENEMY BULLET VS PLAYER COLLISION
            // ----------------------------------------------------
            XMFLOAT3 nextPosFloat;
            XMStoreFloat3(&nextPosFloat, vNextPos);

            if (m_player && bullet->GetHomingTarget() == nullptr && m_player->GetHP() > 0 && !m_player->IsInvincible())
            {
                DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();

                constexpr int ENEMY_BULLET_DAMAGE = 10;
                constexpr float PLAYER_HITBOX_RADIUS = 0.3f; 

                float combinedRadius = PLAYER_HITBOX_RADIUS + bulletRadius;

                // Mathematical CCD (Prevents Tunneling)
                float distToPath = DistancePointToLineSegment2D(currentPos, nextPosFloat, playerPos);

                if (distToPath <= combinedRadius)
                {
                    bool wasAlive = (m_player->GetHP() > 0);
                    m_player->TakeDamage(ENEMY_BULLET_DAMAGE);

                    // ---> THE SIMPLE DEATH STATE <---
                    if (m_player->GetHP() <= 0)
                    {
                        m_player->scale = { 0.0f, 0.0f, 0.0f }; // Make the 3D model vanish
                        m_player->SetInputEnabled(false);       // Stop WASD and Spacebar input
                        m_player->GetMovement()->SetVelocity({ 0,0,0 }); // Stop sliding
                        m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerDead>());
                    }

                    // Destroy the bullet and prevent crashes
                    it = projectiles.erase(it);
                    continue;
                }
            }

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

void CollisionManager::CheckNaviProjectilesVsEnemies(float elapsedTime)
{
    if (!m_navi || !m_enemyManager) return;

    auto& projectiles = m_navi->GetProjectiles();
    auto& enemies = m_enemyManager->GetEnemies();

    constexpr int NAVI_BULLET_DAMAGE = 1;
    constexpr float BULLET_HITBOX_RADIUS = 0.1f;

    for (auto& bullet : projectiles)
    {
        if (!bullet || !bullet->IsActive()) continue;

        DirectX::XMFLOAT3 currentPos = bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 vel = bullet->GetVelocity();

        // ---> CCD MATH: Calculate exactly where the bullet was last frame! <---
        DirectX::XMFLOAT3 prevPos = {
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime), 
            currentPos.z - (vel.z * elapsedTime)
        };

        for (auto& enemy : enemies)
        {
            if (!enemy || !enemy->IsActive()) continue;

            DirectX::XMFLOAT3 ePos = enemy->GetPosition();

            // ---> DYNAMIC HITBOXES <---
            // Fetch the visual scale of the enemy so the hitbox matches the 3D model perfectly
            float enemyScale = enemy->GetScale().x;
            float enemyRadius = 1.0f * enemyScale; // Default Ball

            if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
            else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 0.6f * enemyScale;

            // Combine the enemy's size with the bullet's size
            float exactHitDistance = enemyRadius + BULLET_HITBOX_RADIUS;

            // ---> THE ANTI-TUNNELING CHECK <---
            // Draws an invisible math line from prevPos to currentPos.
            // If the enemy touches ANY part of that line, it's a guaranteed hit!
            float distToPath = DistancePointToLineSegment2D(prevPos, currentPos, ePos);

            if (distToPath <= exactHitDistance)
            {
                enemy->TakeDamage(NAVI_BULLET_DAMAGE);
                bullet->SetActive(false); // Send back to Object Pool instantly
                break; // Stop checking this bullet against other enemies
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

Enemy* CollisionManager::GetTargetInSlashCone(const DirectX::XMFLOAT3& playerPos, const DirectX::XMFLOAT3& aimDir, float reach, float minDotProduct) const
{
    if (!m_enemyManager) return nullptr;

    Enemy* bestTarget{ nullptr };
    float closestDistSq{ (std::numeric_limits<float>::max)() };

    // Range-based for loop. Using const auto& prevents unnecessary deep copies of smart pointers.
    for (const auto& enemy : m_enemyManager->GetEnemies())
    {
        if (!enemy || !enemy->IsActive()) continue;

        DirectX::XMFLOAT3 ePos{ enemy->GetPosition() };
        float enemyScale{ enemy->GetScale().x };
        float enemyRadius{ 1.0f * enemyScale };

        if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
        else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 1.2f * enemyScale;

        float exactSlashDistance{ 0.5f + enemyRadius + reach };
        float exactSlashDistSq{ exactSlashDistance * exactSlashDistance };

        float dx{ ePos.x - playerPos.x };
        float dz{ ePos.z - playerPos.z };
        float distSq{ (dx * dx) + (dz * dz) };

        // BUG ANTICIPATION 1: Fast fail. Check squared distance first to avoid heavy CPU math (sqrt).
        if (distSq < exactSlashDistSq && distSq > 0.0001f)
        {
            // It is near us! Now we do the heavy math to see if we are aiming AT it.
            float dist{ std::sqrt(distSq) };
            float dirX{ dx / dist };
            float dirZ{ dz / dist };

            // Dot Product calculates the angle between our cursor and the enemy.
            // 1.0 means looking dead at them. 0.0 means they are 90 degrees to our side.
            float dot{ (dirX * aimDir.x) + (dirZ * aimDir.z) };

            if (dot >= minDotProduct)
            {
                // BUG ANTICIPATION 2: If 3 enemies are stacked, only hit the closest one!
                if (distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    bestTarget = enemy.get();
                }
            }
        }
    }

    return bestTarget; // Returns nullptr if nothing was aimed at
}

bool CollisionManager::GetParryableProjectile(const XMFLOAT3& playerPos, float threshold, Bullet** outBullet, Enemy** outNearestEnemy)
{
    // 1. Cek Musuh Biasa (JIKA ADA)
    if (m_enemyManager)
    {
        for (auto& enemy : m_enemyManager->GetEnemies())
        {
            if (enemy->GetAttackType() != AttackType::Tracking) continue;

            for (auto& bullet : enemy->GetProjectiles())
            {
                if (!bullet->IsActive()) continue;

                XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

                if (CheckSphereCollision(playerPos, bPos, threshold))
                {
                    if (outBullet) *outBullet = bullet.get();

                    // Cari musuh terdekat untuk dijadikan target homing parry
                    Enemy* nearest = nullptr;
                    float closestDistSq = 999999.0f;
                    for (auto& potential : m_enemyManager->GetEnemies())
                    {
                        if (!potential->IsActive()) continue;
                        XMFLOAT3 targetPos = potential->GetPosition();
                        float distSq = pow(playerPos.x - targetPos.x, 2) + pow(playerPos.z - targetPos.z, 2);
                        if (distSq < closestDistSq) {
                            closestDistSq = distSq;
                            nearest = potential.get();
                        }
                    }
                    if (outNearestEnemy) *outNearestEnemy = nearest ? nearest : enemy.get();

                    return true;
                }
            }
        }
    }

    // =========================================================
    // 2. DETEKSI BIJUUDAMA NAVI BOSS
    // =========================================================
    if (m_naviBoss)
    {
        auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_naviBoss->GetCurrentPhase());
        if (normalPhase)
        {
            for (auto& bullet : normalPhase->GetProjectiles())
            {
                if (!bullet->IsActive()) continue;

                DirectX::XMFLOAT3 vel = bullet->GetVelocity();
                float speedSq = (vel.x * vel.x) + (vel.z * vel.z);

                // [FIX MUTLAK] DI SINILAH TEMPAT YANG BENAR!
                if (speedSq < 0.01f && normalPhase->IsLaserLocked())
                {
                    // Cek apakah pemain menekan Space di dalam Jendela Timing yang pas!
                    float timeDiff = std::abs(normalPhase->GetLaserTimer() - normalPhase->GetParams().laserDuration);
                    if (timeDiff <= normalPhase->GetParams().laserParryWindow)
                    {
                        bullet->SetParryReturn(true);

                        if (outBullet) *outBullet = bullet.get();
                        if (outNearestEnemy) *outNearestEnemy = nullptr;

                        normalPhase->CancelBijuudama(); // Hentikan charge
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void CollisionManager::CheckNaviBossProjectilesVsPlayer(float elapsedTime)
{
    if (!m_naviBoss || !m_player || m_player->GetHP() <= 0) return;

    std::vector<Bullet*> activeBullets;
    auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_naviBoss->GetCurrentPhase());
    auto* wkPhase = dynamic_cast<NaviPhaseWindowkill*>(m_naviBoss->GetCurrentPhase());

    if (normalPhase) {
        for (auto& b : normalPhase->GetProjectiles()) activeBullets.push_back(b.get());
    }
    else if (wkPhase) {
        activeBullets = wkPhase->GetProjectiles();
    }

    if (activeBullets.empty()) return;

    constexpr float PLAYER_HURTBOX_RADIUS = 0.3f;
    constexpr int BOSS_BULLET_DAMAGE = 10;

    // =========================================================
    // [FIX MUTLAK 1] GUNAKAN 'activeBullets', BUKAN 'normalPhase'!
    // =========================================================
    for (Bullet* bullet : activeBullets)
    {
        if (!bullet || !bullet->IsActive()) continue;

        DirectX::XMFLOAT3 currentPos = bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 vel = bullet->GetVelocity();
        float speedSq = (vel.x * vel.x) + (vel.z * vel.z);

        // TIER 3: Peluru sukses dipantulkan ke bos
        if (speedSq > 10000.0f) continue;

        DirectX::XMFLOAT3 prevPos = {
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime),
            currentPos.z - (vel.z * elapsedTime)
        };

        DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();
        float combinedRadius = PLAYER_HURTBOX_RADIUS + bullet->GetRadius();
        float distToPath = DistancePointToLineSegment2D(prevPos, currentPos, playerPos);

        // TIER 2: BIJUUDAMA YANG SUKSES DI-PARRY
        if (bullet->IsParryReturn())
        {
            if (distToPath <= combinedRadius)
            {
                TimeManager::Instance().TriggerHitStop(0.15f, 0.0f);
                CameraController::Instance().AddTrauma(0.8f);

                // Pastikan shatter hanya dipanggil jika memang normalPhase ada
                if (normalPhase) {
                    normalPhase->ShatterBijuudama(currentPos, m_naviBoss);
                }

                bullet->SetActive(false);
                AudioManager::Instance().PlaySFX("Data/Sound/SE_Parry.wav", 0.8);
            }
            continue;
        }

        // =========================================================
        // TIER 1: Peluru Normal / Bijuudama GAGAL Parry
        // =========================================================
        if (distToPath <= combinedRadius)
        {
            if (!m_player->IsInvincible()) {
                m_player->TakeDamage(BOSS_BULLET_DAMAGE);

                // [FIX MUTLAK] HANYA hancurkan peluru jika ini dari Fase Normal!
                // Peluru Windowkill (wkPhase) akan dibiarkan hidup dan terus memantul!
                if (normalPhase) {
                    bullet->SetActive(false);
                }

                if (m_player->GetHP() <= 0)
                {
                    m_player->scale = { 0.0f, 0.0f, 0.0f };
                    m_player->SetInputEnabled(false);
                    m_player->GetMovement()->SetVelocity({ 0,0,0 });
                    m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerDead>());
                }
            }
        }
    }

    // =========================================================
    // [FIX MUTLAK 2] PASTIKAN 'normalPhase' VALID SEBELUM CEK HUJAN!
    // =========================================================
    if (normalPhase && normalPhase->GetRainState() == 2) {
        DirectX::XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 rCenter = normalPhase->GetRainCenter();

        float halfW = normalPhase->GetActualRainWidth() * 0.5f;
        float halfD = normalPhase->GetActualRainDepth() * 0.5f;

        if (pPos.x > (rCenter.x - halfW) && pPos.x < (rCenter.x + halfW) &&
            pPos.z >(rCenter.z - halfD) && pPos.z < (rCenter.z + halfD))
        {
            if (!m_player->IsInvincible()) {
                m_player->TakeDamage(normalPhase->GetParams().rainDamage);

                if (m_player->GetHP() <= 0) {
                    m_player->scale = { 0.0f, 0.0f, 0.0f };
                    m_player->SetInputEnabled(false);
                    m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerDead>());
                }
            }
        }
    }

    // =========================================================
    // [NEW] DETEKSI DAMAGE LASER (AREA DENIAL)
    // =========================================================
    if (wkPhase) {
        auto blaster = wkPhase->GetBlaster();

        // Damage HANYA diberikan saat State = 3 (Firing)
        if (blaster.active && blaster.state == 3) {
            DirectX::XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();

            // Hitung Area Laser (Mencontek logika Rain)
            // Sumbu X: Berdasarkan beamScaleX (dikali 6 seperti di Render)
            float halfWidth = (blaster.beamScaleX * 6.0f) * 0.5f;

            // Sumbu Z: Laser memanjang dari posisi kepala ke arah negatif Z (belakang)
            float laserLength = 120.0f;
            float zStart = blaster.pos.z;
            float zEnd = blaster.pos.z - laserLength;

            // Cek apakah Player di dalam kotak laser (XZ Plane)
            if (pPos.x > (blaster.pos.x - halfWidth) && pPos.x < (blaster.pos.x + halfWidth) &&
                pPos.z < zStart && pPos.z > zEnd)
            {
                if (!m_player->IsInvincible()) {
                    // Berikan damage besar karena ini Laser!
                    m_player->TakeDamage(20);

                    if (m_player->GetHP() <= 0) {
                        m_player->scale = { 0.0f, 0.0f, 0.0f };
                        m_player->SetInputEnabled(false);
                        m_player->GetStateMachine()->ChangeState(m_player, std::make_unique<PlayerDead>());
                    }
                }
            }
        }
    }
}

void CollisionManager::CheckNaviBossProjectilesVsBoss(float elapsedTime)
{
    if (!m_naviBoss) return;
    auto* normalPhase = dynamic_cast<NaviPhaseNormal*>(m_naviBoss->GetCurrentPhase());
    if (!normalPhase) return;

    for (auto& bullet : normalPhase->GetProjectiles())
    {
        if (!bullet->IsActive()) continue;

        // Bandingkan BossTarget dengan m_naviBoss
        if (bullet->GetBossTarget() == m_naviBoss)
        {
            DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
            DirectX::XMFLOAT3 bossPos = m_naviBoss->GetPosition();

            // =========================================================
            // [FIX 1] DETEKSI 3D PENUH
            // Tambahkan sumbu Y agar pecahan parabola tidak meledak di udara
            // saat melintas tepat di atas kepala bos.
            // =========================================================
            float dx = bPos.x - bossPos.x;
            float dy = bPos.y - bossPos.y;
            float dz = bPos.z - bossPos.z;
            float distSq = (dx * dx) + (dy * dy) + (dz * dz);

            // Hitbox Boss untuk serangan ini (radius 4.0f -> Kuadrat = 16.0f)
            if (distSq <= 16.0f)
            {
                // 1. Matikan kepingan peluru agar tidak hit berkali-kali
                bullet->SetActive(false);

                // =========================================================
                // [FIX 2] ROUTING DAMAGE YANG BENAR
                // Panggil TakeDamage langsung ke Fase-nya agar sinkron 
                // dengan UI Bar, Efek Suara, dan Flash Damage!
                // =========================================================
                normalPhase->TakeDamage(15);

                // 3. [JUICE] Berikan micro-shake untuk SETIAP kepingan yang menabrak
                CameraController::Instance().AddTrauma(0.15f);
            }
        }
    }
}