#include "Enemy.h"

using namespace DirectX;

Enemy::Enemy(ID3D11Device* device, const char* filePath, XMFLOAT3 startPos, XMFLOAT3 startRot,
    XMFLOAT4 startColor, EnemyType type, AttackType attackType,
    float minX, float maxX, float minZ, float maxZ, MoveDir dir)
{
    m_model = std::make_shared<Model>(device, filePath);
    model = m_model;
    m_type = type;
    m_attackType = attackType;

    if (m_type == EnemyType::Pentagon) m_scale = { 150.0f, 150.0f, 150.0f };
    else m_scale = { 1.0f, 1.0f, 1.0f };

    movement->SetGravityEnabled(false);
    movement->SetPosition(startPos);
    movement->SetRotation(startRot);
    originalPosition = startPos;
    originalRotation = startRot;
    this->color = startColor;
    m_patrolMinX = startPos.x + minX;
    m_patrolMaxX = startPos.x + maxX;
    m_patrolMinZ = startPos.z + minZ;
    m_patrolMaxZ = startPos.z + maxZ;
    m_randomTargetPos = startPos;
    if (dir == MoveDir::Right)      m_currentSpeed = -m_baseMoveSpeed;
    else if (dir == MoveDir::Left)  m_currentSpeed = m_baseMoveSpeed;
    else                            m_currentSpeed = 0.0f;
    m_moveDir = dir;
    m_isActive = true;
}

float Enemy::GetRandomFloat(float min, float max)
{
    float random = ((float)rand()) / (float)RAND_MAX;
    float range = max - min;
    return (random * range) + min;
}

Enemy::~Enemy() {}

void Enemy::Update(float elapsedTime, Camera* camera)
{
    if (m_type == EnemyType::Pentagon)
    {
        XMFLOAT3 rot = movement->GetRotation();
        rot.y += 10.0f * elapsedTime;
        movement->SetRotation(rot);
    }

    if (m_attackType == AttackType::TrackingHorizontal)
    {
        XMFLOAT3 pos = movement->GetPosition();
        pos.x += m_currentSpeed * elapsedTime;

        if (pos.x >= m_patrolMaxX)
        {
            pos.x = m_patrolMaxX;
            m_currentSpeed = -std::abs(m_baseMoveSpeed);
        }
        else if (pos.x <= m_patrolMinX)
        {
            pos.x = m_patrolMinX;
            m_currentSpeed = std::abs(m_baseMoveSpeed);
        }
        movement->SetPosition(pos);
    }
    else if (m_attackType == AttackType::TrackingRandom)
    {
        XMFLOAT3 pos = movement->GetPosition();
        float dx = m_randomTargetPos.x - pos.x;
        float dz = m_randomTargetPos.z - pos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < 0.1f)
        {
            m_randomTargetPos.x = GetRandomFloat(m_patrolMinX, m_patrolMaxX);
            m_randomTargetPos.z = GetRandomFloat(m_patrolMinZ, m_patrolMaxZ);
            m_randomTargetPos.y = pos.y;
        }

        XMVECTOR vPos = XMLoadFloat3(&pos);
        XMVECTOR vTarget = XMLoadFloat3(&m_randomTargetPos);
        XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
        vDir = XMVector3Normalize(vDir);
        XMVECTOR vOffset = vDir * m_baseMoveSpeed * elapsedTime;
        XMVECTOR vNewPos = XMVectorAdd(vPos, vOffset);
        XMStoreFloat3(&pos, vNewPos);
        movement->SetPosition(pos);
    }
    else if (m_attackType == AttackType::Tracking)
    {
        if (m_baseMoveSpeed > 0.0f)
        {
            XMFLOAT3 fwd = GetForwardVector();
            float chaseSpeed = m_baseMoveSpeed * 0.4f;
            XMFLOAT3 pos = movement->GetPosition();
            pos.x += fwd.x * chaseSpeed * elapsedTime;
            pos.z += fwd.z * chaseSpeed * elapsedTime;
            movement->SetPosition(pos);
        }
    }

    if (movement) movement->Update(elapsedTime);

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z)
    );
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    if (m_model) m_model->UpdateTransform(worldMatrix);
}

void Enemy::UpdateTracking(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos, bool allowAttack)
{
    if (!camera) return;
    if (allowAttack)
    {
        UpdateAttackLogic(elapsedTime, camera, playerPos, allowAttack);
    }
}

void Enemy::UpdateAttackLogic(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos, bool allowAttack)
{
    if (m_attackType == AttackType::None) return;
    if (!camera) return;

    XMFLOAT3 myPos = movement->GetPosition();
    XMFLOAT3 targetPos = playerPos;

    float dx = std::abs(targetPos.x - myPos.x);
    float dz = std::abs(targetPos.z - myPos.z);
    float distSq = dx * dx + dz * dz;

    if (distSq < (m_activationDistance * m_activationDistance))
    {
        bool isTrackingType = (m_attackType == AttackType::Tracking ||
            m_attackType == AttackType::TrackingHorizontal ||
            m_attackType == AttackType::TrackingRandom);

        if (isTrackingType)
        {
            float diffX = targetPos.x - myPos.x;
            float diffZ = targetPos.z - myPos.z;
            float targetYawRad = atan2f(diffX, diffZ);
            float targetYawDeg = XMConvertToDegrees(targetYawRad);
            movement->SetRotation({ 0.0f, targetYawDeg, 0.0f });
        }

        m_attackTimer += elapsedTime;

        if (m_attackTimer >= m_fireRate)
        {
            m_attackTimer = 0.0f;

            if (m_attackType == AttackType::RadialBurst)
            {
                int projectileCount = 8;
                float angleStep = DirectX::XM_2PI / projectileCount;

                for (int i = 0; i < projectileCount; ++i)
                {
                    float currentAngle = i * angleStep;
                    float dirX = sinf(currentAngle);
                    float dirZ = cosf(currentAngle);
                    XMFLOAT3 burstDir = { dirX, 0.0f, dirZ };

                    auto newBullet = std::make_unique<Bullet>();
                    XMFLOAT3 spawnPos = myPos;
                    spawnPos.x += dirX * 1.0f;
                    spawnPos.z += dirZ * 1.0f;

                    newBullet->Fire(spawnPos, burstDir, m_projectileSpeed);
                    m_projectiles.push_back(std::move(newBullet));
                }
            }
            else
            {
                if (m_projectiles.size() >= MAX_PROJECTILES) m_projectiles.pop_front();

                XMFLOAT3 fwd;
                if (isTrackingType)
                {
                    XMVECTOR vTarget = XMLoadFloat3(&targetPos);
                    XMVECTOR vStart = XMLoadFloat3(&myPos);
                    XMVECTOR vDir = XMVectorSubtract(vTarget, vStart);
                    vDir = XMVector3Normalize(vDir);
                    XMStoreFloat3(&fwd, vDir);
                }
                else
                {
                    fwd = GetForwardVector();
                }

                auto newBullet = std::make_unique<Bullet>();
                XMFLOAT3 spawnPos = myPos;
                spawnPos.x += fwd.x * SPAWN_OFFSET_FWD;
                spawnPos.z += fwd.z * SPAWN_OFFSET_FWD;
                spawnPos.y += SPAWN_OFFSET_Y;

                newBullet->Fire(spawnPos, fwd, m_projectileSpeed);
                m_projectiles.push_back(std::move(newBullet));
            }
        }
    }

    auto it = m_projectiles.begin();
    while (it != m_projectiles.end())
    {
        auto& bullet = *it;
        bullet->Update(elapsedTime, camera);

        XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
        float bDx = myPos.x - bPos.x;
        float bDz = myPos.z - bPos.z;

        if ((bDx * bDx + bDz * bDz) > (m_despawnDistance * m_despawnDistance)) it = m_projectiles.erase(it);
        else ++it;
    }
}

DirectX::XMFLOAT3 Enemy::GetForwardVector() const
{
    XMFLOAT3 rot = movement->GetRotation();
    float yawRad = XMConvertToRadians(rot.y);
    float pitchRad = XMConvertToRadians(rot.x);
    float x = sinf(yawRad) * cosf(pitchRad);
    float y = -sinf(pitchRad);
    float z = cosf(yawRad) * cosf(pitchRad);
    return { x, y, z };
}

void Enemy::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
    {
        if (bullet->IsActive()) renderer->Draw(ShaderId::Phong, bullet->GetModel(), m_projectileColor);
    }
}

void Enemy::RenderDebugProjectiles(ShapeRenderer* renderer)
{
    for (const auto& bullet : m_projectiles)
    {
        if (bullet && bullet->IsActive())
        {
            DirectX::XMFLOAT3 pos = bullet->GetMovement()->GetPosition();
            float radius = bullet->GetRadius();
            renderer->DrawSphere(pos, radius, { 1.0f, 0.0f, 0.0f, 1.0f });
        }
    }

    if (m_isHighlighted)
    {
        DirectX::XMFLOAT3 pos = movement->GetPosition();
        renderer->DrawBox(pos, { 0.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
    }
}

void Enemy::SetPatrolLimitsX(float minOffset, float maxOffset)
{
    m_patrolMinX = originalPosition.x + minOffset;
    m_patrolMaxX = originalPosition.x + maxOffset;
}

void Enemy::SetPatrolLimitsZ(float minOffset, float maxOffset)
{
    m_patrolMinZ = originalPosition.z + minOffset;
    m_patrolMaxZ = originalPosition.z + maxOffset;
}

void Enemy::UpdateOriginalTransform(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot)
{
    originalPosition = pos;
    originalRotation = rot;

    if (!m_isActive)
    {
        m_patrolMinX = pos.x; m_patrolMaxX = pos.x;
        m_patrolMinZ = pos.z; m_patrolMaxZ = pos.z;
    }
}

void Enemy::SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); }
void Enemy::SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

DirectX::XMFLOAT3 Enemy::GetPosition() const { return movement->GetPosition(); }
DirectX::XMFLOAT3 Enemy::GetRotation() const { return movement->GetRotation(); }