#include "Enemy.h"

using namespace DirectX;

Enemy::Enemy(ID3D11Device* device, const char* filePath, XMFLOAT3 startPos, XMFLOAT3 startRot, XMFLOAT4 startColor, EnemyType type, AttackType attackType)
{
    m_model = std::make_shared<Model>(device, filePath);
    model = m_model; 
    m_type = type;
    m_attackType = attackType;

    // Default Transform
    movement->SetPosition(startPos);
    movement->SetRotation(startRot);
    originalPosition = startPos;
    originalRotation = startRot;
    this->color = startColor;
}

Enemy::~Enemy(){}

void Enemy::Update(float elapsedTime, Camera* camera)
{
    if (movement) movement->Update(elapsedTime);
    if (m_attackType != AttackType::None) { UpdateAttackLogic(elapsedTime, camera); }
    SyncData();
}

void Enemy::UpdateAttackLogic(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    XMFLOAT3 camPos = camera->GetPosition();
    XMFLOAT3 myPos = movement->GetPosition();

    float dx = std::abs(camPos.x - myPos.x);
    float dz = std::abs(camPos.z - myPos.z);
    float distSq = dx * dx + dz * dz;

    // Only fire if within range (Visible) AND not too far behind
    if (distSq < (m_activationDistance * m_activationDistance))
    {
        m_attackTimer += elapsedTime;

        if (m_attackTimer >= m_fireRate)
        {
            m_attackTimer = 0.0f;

            if (m_projectiles.size() >= MAX_PROJECTILES)
            {
                m_projectiles.pop_front();
            }

            auto newBall = std::make_unique<Ball>();
            XMFLOAT3 fwd = GetForwardVector();
            XMFLOAT3 spawnPos = myPos;
            spawnPos.x += fwd.x * SPAWN_OFFSET_FWD;
            spawnPos.z += fwd.z * SPAWN_OFFSET_FWD;
            spawnPos.y += SPAWN_OFFSET_Y;
            newBall->Fire(spawnPos, fwd, m_projectileSpeed);
            m_projectiles.push_back(std::move(newBall));
        }
    }

    // Projectile Update & Clean Up
    auto it = m_projectiles.begin();
    while (it != m_projectiles.end())
    {
        auto& ball = *it;
        ball->Update(elapsedTime, camera); 

        // Check THIS ball's distance to camera
        XMFLOAT3 ballPos = ball->GetMovement()->GetPosition();
        float bDx = camPos.x - ballPos.x;
        float bDz = camPos.z - ballPos.z;
        float ballDistSq = bDx * bDx + bDz * bDz;

        if (ballDistSq > (m_despawnDistance * m_despawnDistance))
        {
            it = m_projectiles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

DirectX::XMFLOAT3 Enemy::GetForwardVector() const
{
    XMFLOAT3 rot = movement->GetRotation();
    float yaw = rot.y;
    float pitch = rot.x;
    float x = sinf(yaw) * cosf(pitch);
    float y = -sinf(pitch);
    float z = cosf(yaw) * cosf(pitch);
    return { x, y, z };
}

void Enemy::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& ball : m_projectiles)
    {
        if (ball->IsActive())
        {
            renderer->Draw(ShaderId::Phong, ball->GetModel(), m_projectileColor);
        }
    }
}

void Enemy::SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); }
void Enemy::SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

DirectX::XMFLOAT3 Enemy::GetPosition() const { return movement->GetPosition(); }
DirectX::XMFLOAT3 Enemy::GetRotation() const { return movement->GetRotation(); }