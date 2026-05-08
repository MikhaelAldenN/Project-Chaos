#include "NaviAlly.h"
#include "Player.h"
#include "EnemyManager.h"
#include "Enemy.h"
#include "Camera.h"
#include "System/Graphics.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

NaviAlly::NaviAlly(ID3D11Device* device, const Player* targetPlayer, EnemyManager* enemyManager)
    : m_targetPlayer(targetPlayer), m_enemyManager(enemyManager)
{
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");
    scale = { 1.4f, 1.4f, 1.4f };
    movement->SetRotation({ 0.0f, 0.0f, 0.0f });

    if (m_targetPlayer)
    {
        XMFLOAT3 startPos{ m_targetPlayer->GetPosition() };
        startPos.x += 1.0f;
        startPos.y += HOVER_HEIGHT;
        startPos.z += 0.5f;
        movement->SetPosition(startPos);
    }
    SyncData();
}

void NaviAlly::Update(float elapsedTime, Camera* camera)
{
    UpdateHoverLogic(elapsedTime);
    UpdateShootingLogic(elapsedTime, camera);
    UpdateProjectiles(elapsedTime, camera);
}

void NaviAlly::UpdateHoverLogic(float elapsedTime)
{
    if (!m_targetPlayer) return;

    m_animTime += elapsedTime;

    XMFLOAT3 playerPos{ m_targetPlayer->GetPosition() };
    // Target position: Right shoulder
    XMFLOAT3 targetPos{ playerPos.x + 1.0f, playerPos.y + HOVER_HEIGHT, playerPos.z + 0.5f };
    XMFLOAT3 currentPos{ movement->GetPosition() };

    // Framerate-independent smoothing
    float lerpFactor{ 1.0f - std::expf(-FOLLOW_SPEED * elapsedTime) };

    currentPos.x += (targetPos.x - currentPos.x) * lerpFactor;
    currentPos.z += (targetPos.z - currentPos.z) * lerpFactor;
    currentPos.y = targetPos.y + (std::sinf(m_animTime * FLOAT_SPEED) * FLOAT_AMP);

    movement->SetPosition(currentPos);
    SyncData();
}

void NaviAlly::UpdateShootingLogic(float elapsedTime, Camera* camera)
{
    // Don't shoot if missing dependencies
    if (!m_enemyManager || !camera) return;

    m_fireTimer += elapsedTime;
    if (m_fireTimer < FIRE_RATE) return;

    XMFLOAT3 myPos{ movement->GetPosition() };
    float closestDistSq{ ATTACK_RANGE_SQ }; // Start at max allowed distance
    Enemy* targetEnemy{ nullptr };

    // Find the closest active enemy using purely cheap Distance math
    for (const auto& enemy : m_enemyManager->GetEnemies())
    {
        // Ignore dead enemies
        if (!enemy || !enemy->IsActive()) continue;

        XMFLOAT3 ePos{ enemy->GetPosition() };
        float dx{ ePos.x - myPos.x };
        float dz{ ePos.z - myPos.z };
        float distSq{ (dx * dx) + (dz * dz) };

        if (distSq < closestDistSq)
        {
            // CPU OPTIMIZATION: ONLY do the heavy Camera Frustum matrix math 
            // if the enemy is actually close enough to hit!
            float dynamicRadius = 1.5f * enemy->GetScale().x;
            if (camera->CheckSphere(ePos.x, ePos.y, ePos.z, dynamicRadius))
            {
                closestDistSq = distSq;
                targetEnemy = enemy.get();
            }
        }
    }

    // Fire instantly in the same frame (prevents dangling pointer crashes)
    if (targetEnemy)
    {
        m_fireTimer = 0.0f;
        FireAtTarget(targetEnemy->GetPosition());
    }
}

void NaviAlly::FireAtTarget(const XMFLOAT3& targetPos)
{
    XMFLOAT3 myPos{ movement->GetPosition() };
    XMFLOAT3 aimPoint{ targetPos.x, targetPos.y, targetPos.z };

    // Calculate the 3D difference
    float dx{ aimPoint.x - myPos.x };
    float dy{ aimPoint.y - myPos.y }; 
    float dz{ aimPoint.z - myPos.z };

    float distSq{ (dx * dx) + (dy * dy) + (dz * dz) };

    // BUG PREVENTION: The "Divide-By-Zero" Guard
    // If the enemy overlaps exactly with Navi, normal math will crash the engine (NaN).
    XMFLOAT3 fwd{ 0.0f, 0.0f, 1.0f }; // Default safe vector
    if (distSq > 0.0001f)
    {
        // Calculate full 3D direction
        float dist{ std::sqrtf(distSq) };
        fwd = { dx / dist, dy / dist, dz / dist };
    }

    // ----------------------------------------------------
    // TRUE OBJECT POOL: Recycle dead bullets
    // ----------------------------------------------------
    for (auto& bullet : m_projectiles)
    {
        if (!bullet->IsActive())
        {
            bullet->Fire(myPos, fwd, BULLET_SPEED);
            return; // Exit instantly, zero runtime allocation cost
        }
    }

    // Only allocate new memory if every bullet is currently in-flight
    auto newBullet{ std::make_unique<Bullet>() };
    newBullet->scale = { 0.7f, 0.7f, 0.7f };
    newBullet->Fire(myPos, fwd, BULLET_SPEED);
    m_projectiles.push_back(std::move(newBullet));

    // Cap the memory pool
    if ((int)m_projectiles.size() > MAX_BULLETS) m_projectiles.pop_front();
}

void NaviAlly::UpdateProjectiles(float elapsedTime, Camera* camera)
{
    XMFLOAT3 myPos{ movement->GetPosition() };

    for (auto& bullet : m_projectiles)
    {
        if (!bullet->IsActive()) continue;

        bullet->Update(elapsedTime, camera);

        // INFINITE FLIGHT GUARD: Turn off (recycle) if it flies too far
        XMFLOAT3 bPos{ bullet->GetMovement()->GetPosition() };
        float dx{ myPos.x - bPos.x };
        float dz{ myPos.z - bPos.z };

        if ((dx * dx + dz * dz) > DESPAWN_DIST_SQ)
        {
            bullet->SetActive(false);
        }
    }
}

void NaviAlly::Render(ModelRenderer* renderer)
{
    if (!model) return;
    Character::scale = scale;
    renderer->Draw(ShaderId::Phong, model, m_color);
}

void NaviAlly::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
    {
        if (bullet->IsActive()) renderer->Draw(ShaderId::Phong, bullet->GetModel(), m_color);
    }
}