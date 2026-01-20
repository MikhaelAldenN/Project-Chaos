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
    if (dir == MoveDir::Right)      m_currentSpeed = m_baseMoveSpeed;  
    else if (dir == MoveDir::Left)  m_currentSpeed = -m_baseMoveSpeed; 
    else                            m_currentSpeed = 0.0f;
}

float Enemy::GetRandomFloat(float min, float max)
{
    float random = ((float)rand()) / (float)RAND_MAX;
    float range = max - min;
    return (random * range) + min;
}

Enemy::~Enemy(){}

void Enemy::Update(float elapsedTime, Camera* camera)
{
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

    if (movement) movement->Update(elapsedTime);
    SyncData();
}

void Enemy::UpdateTracking(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos)
{
    if (!camera) return;
    UpdateAttackLogic(elapsedTime, camera, playerPos);
}

void Enemy::UpdateAttackLogic(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos)
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
            float targetYaw = atan2f(diffX, diffZ);
            movement->SetRotationY(targetYaw);
        }

        m_attackTimer += elapsedTime;

        if (m_attackTimer >= m_fireRate)
        {
            m_attackTimer = 0.0f;

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

            auto newBall = std::make_unique<Ball>();
            XMFLOAT3 spawnPos = myPos;
            spawnPos.x += fwd.x * SPAWN_OFFSET_FWD;
            spawnPos.z += fwd.z * SPAWN_OFFSET_FWD;
            spawnPos.y += SPAWN_OFFSET_Y;

            newBall->Fire(spawnPos, fwd, m_projectileSpeed);
            m_projectiles.push_back(std::move(newBall));
        }
    }

    auto it = m_projectiles.begin();
    while (it != m_projectiles.end())
    {
        auto& ball = *it;
        ball->Update(elapsedTime, camera);

        XMFLOAT3 bPos = ball->GetMovement()->GetPosition();
        float bDx = targetPos.x - bPos.x;
        float bDz = targetPos.z - bPos.z;

        if ((bDx * bDx + bDz * bDz) > (m_despawnDistance * m_despawnDistance)) it = m_projectiles.erase(it);
        else ++it;
    }
}

DirectX::XMFLOAT3 Enemy::GetForwardVector() const
{
    XMFLOAT3 rot = movement->GetRotation();
    float yaw = rot.y; float pitch = rot.x;
    float x = sinf(yaw) * cosf(pitch); float y = -sinf(pitch); float z = cosf(yaw) * cosf(pitch);
    return { x, y, z };
}

void Enemy::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& ball : m_projectiles)
    {
        if (ball->IsActive()) renderer->Draw(ShaderId::Phong, ball->GetModel(), m_projectileColor);
    }
}

void Enemy::SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); }
void Enemy::SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

DirectX::XMFLOAT3 Enemy::GetPosition() const { return movement->GetPosition(); }
DirectX::XMFLOAT3 Enemy::GetRotation() const { return movement->GetRotation(); }