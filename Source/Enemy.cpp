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

    if (m_type == EnemyType::Pentagon)
    {
        // Pentagon agak besar (misal 2x lipat)
        m_scale = { 150.0f, 150.0f, 150.0f };
    }
    else
    {
        // Default normal
        m_scale = { 1.0f, 1.0f, 1.0f };
    }

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

Enemy::~Enemy() {}

void Enemy::Update(float elapsedTime, Camera* camera)
{
    // =========================================================
    // [BARU] LOGIKA ROTASI PENTAGON
    // =========================================================
    if (m_type == EnemyType::Pentagon)
    {
        // Putar musuh secara terus menerus pada sumbu Y
        XMFLOAT3 rot = movement->GetRotation();
        rot.y += 10.0f * elapsedTime; // Kecepatan putar (makin besar makin cepat)
        movement->SetRotation(rot);
    }
    // =========================================================

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
        XMFLOAT3 fwd = GetForwardVector();

        float chaseSpeed = m_baseMoveSpeed * 0.4f;

        XMFLOAT3 pos = movement->GetPosition();
        pos.x += fwd.x * chaseSpeed * elapsedTime;
        pos.z += fwd.z * chaseSpeed * elapsedTime;

        movement->SetPosition(pos);
    }

    if (movement) movement->Update(elapsedTime);
    //SyncData();

    // =========================================================
    // [BARU] APPLY SCALE & TRANSFORM MANUAL
    // =========================================================
    // Kita harus hitung ulang matriks dunia (World Matrix) agar scale-nya masuk.

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    // 2. Buat Matrix Scale (INILAH KUNCINYA)
    XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);

    // 3. Buat Matrix Rotasi
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z)
    );

    // 4. Buat Matrix Posisi
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 5. Gabungkan: Scale -> Rotate -> Translate
    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    // 6. Terapkan ke Model
    if (m_model) m_model->UpdateTransform(worldMatrix);
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
    XMFLOAT3 targetPos = playerPos; // Default target

    // Jarak ke player (untuk aktivasi)
    float dx = std::abs(targetPos.x - myPos.x);
    float dz = std::abs(targetPos.z - myPos.z);
    float distSq = dx * dx + dz * dz;

    // Cek apakah player cukup dekat untuk mulai menyerang
    if (distSq < (m_activationDistance * m_activationDistance))
    {
        bool isTrackingType = (m_attackType == AttackType::Tracking ||
            m_attackType == AttackType::TrackingHorizontal ||
            m_attackType == AttackType::TrackingRandom);

        // Jika tipe Tracking, putar badan menghadap player
        if (isTrackingType)
        {
            float diffX = targetPos.x - myPos.x;
            float diffZ = targetPos.z - myPos.z;

            // atan2f mengembalikan RADIANS (-3.14 s/d 3.14)
            float targetYawRad = atan2f(diffX, diffZ);

            // [FIX] Konversi ke DEGREES agar sesuai dengan sistem render
            float targetYawDeg = XMConvertToDegrees(targetYawRad);

            movement->SetRotation({ 0.0f, targetYawDeg, 0.0f });
        }

        m_attackTimer += elapsedTime;

        if (m_attackTimer >= m_fireRate)
        {
            m_attackTimer = 0.0f;

            // =========================================================
            // [BARU] LOGIKA SERANGAN RADIAL BURST (PENTAGON)
            // =========================================================
            if (m_attackType == AttackType::RadialBurst)
            {
                int projectileCount = 8; // Jumlah peluru (8 penjuru mata angin)
                float angleStep = DirectX::XM_2PI / projectileCount; // 360 derajat / 8

                for (int i = 0; i < projectileCount; ++i)
                {
                    // Hitung arah peluru berdasarkan sudut lingkaran
                    float currentAngle = i * angleStep;
                    float dirX = sinf(currentAngle);
                    float dirZ = cosf(currentAngle);

                    XMFLOAT3 burstDir = { dirX, 0.0f, dirZ };

                    // Spawn Projectile
                    auto newBall = std::make_unique<Ball>();

                    // Offset spawn sedikit biar gak numpuk di tengah badan musuh
                    XMFLOAT3 spawnPos = myPos;
                    spawnPos.x += dirX * 1.0f;
                    spawnPos.z += dirZ * 1.0f;

                    newBall->Fire(spawnPos, burstDir, m_projectileSpeed);
                    newBall->SetBoundariesEnabled(false); // Biar peluru bisa tembus tembok layar

                    // Masukkan ke list peluru
                    m_projectiles.push_back(std::move(newBall));
                }
            }
            // =========================================================
            // LOGIKA SERANGAN NORMAL (TRACKING / STATIC)
            // =========================================================
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

                auto newBall = std::make_unique<Ball>();
                XMFLOAT3 spawnPos = myPos;
                spawnPos.x += fwd.x * SPAWN_OFFSET_FWD;
                spawnPos.z += fwd.z * SPAWN_OFFSET_FWD;
                spawnPos.y += SPAWN_OFFSET_Y;

                newBall->Fire(spawnPos, fwd, m_projectileSpeed);
                newBall->SetBoundariesEnabled(false);

                m_projectiles.push_back(std::move(newBall));
            }
        }
    }

    // Update Projectiles (Hapus jika terlalu jauh)
    auto it = m_projectiles.begin();
    while (it != m_projectiles.end())
    {
        auto& ball = *it;

        ball->Update(elapsedTime, camera);

        XMFLOAT3 bPos = ball->GetMovement()->GetPosition();

        // Gunakan myPos (posisi musuh) sebagai titik tengah despawn distance
        float bDx = myPos.x - bPos.x;
        float bDz = myPos.z - bPos.z;

        if ((bDx * bDx + bDz * bDz) > (m_despawnDistance * m_despawnDistance)) it = m_projectiles.erase(it);
        else ++it;
    }
}

DirectX::XMFLOAT3 Enemy::GetForwardVector() const
{
    XMFLOAT3 rot = movement->GetRotation();

    // [FIX] Konversi Derajat ke Radian sebelum masuk sin/cos!
    float yawRad = XMConvertToRadians(rot.y);
    float pitchRad = XMConvertToRadians(rot.x);

    // Gunakan nilai Radian untuk menghitung vektor
    float x = sinf(yawRad) * cosf(pitchRad);
    float y = -sinf(pitchRad);
    float z = cosf(yawRad) * cosf(pitchRad);

    return { x, y, z };
}

void Enemy::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& ball : m_projectiles)
    {
        if (ball->IsActive()) renderer->Draw(ShaderId::Phong, ball->GetModel(), m_projectileColor);
    }
}

void Enemy::RenderDebugProjectiles(ShapeRenderer* renderer)
{
    for (const auto& ball : m_projectiles)
    {
        if (ball && ball->IsActive())
        {
            DirectX::XMFLOAT3 pos = ball->GetMovement()->GetPosition();
            float radius = ball->GetRadius();
            renderer->DrawSphere(pos, radius, { 1.0f, 0.0f, 0.0f, 1.0f });
        }
    }
}

void Enemy::SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); }
void Enemy::SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

DirectX::XMFLOAT3 Enemy::GetPosition() const { return movement->GetPosition(); }
DirectX::XMFLOAT3 Enemy::GetRotation() const { return movement->GetRotation(); }