#include "Enemy.h"
#include "EnemyManager.h"

using namespace DirectX;

EnemyManager::EnemyManager() {}

EnemyManager::~EnemyManager() { m_enemies.clear(); }

void EnemyManager::Initialize(ID3D11Device* device)
{
    for (const auto& config : EnemyLevelData::Spawns)
    {
        SpawnEnemy(config);
    }
}

void EnemyManager::SpawnEnemy(const EnemySpawnConfig& config)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    const char* modelPath = "";

    // [MODIFIKASI] Seleksi Model
    if (config.Type == EnemyType::Ball)
    {
        modelPath = "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb";
    }
    else if (config.Type == EnemyType::Pentagon) // [BARU]
    {
        modelPath = "Data/Model/Character/PLACEHOLDER_mdl_Pentagon.glb";
    }
    else
    {
        // Default ke Paddle
        modelPath = "Data/Model/Character/PLACEHOLDER_mdl_Paddle.glb";
    }

    auto newEnemy = std::make_unique<Enemy>(
        device,
        modelPath,
        config.Position,
        config.Rotation,
        config.Color,
        config.Type,
        config.AttackBehavior,
        config.MinX,
        config.MaxX,
        config.MinZ,
        config.MaxZ,
        config.Direction
    );

    newEnemy->SetScale(config.Scale);
    newEnemy->SetBaseMoveSpeed(config.BaseSpeed);
    m_enemies.push_back(std::move(newEnemy));
}

void EnemyManager::Update(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos, bool allowAttack)
{
    for (auto& enemy : m_enemies)
    {
        enemy->Update(elapsedTime, camera);
        enemy->UpdateTracking(elapsedTime, camera, playerPos, allowAttack);
    }
}

void EnemyManager::Render(ModelRenderer* renderer, Camera* camera)
{
    for (auto& enemy : m_enemies)
    {
        bool isBodyVisible = true;

        if (camera)
        {
            DirectX::XMFLOAT3 pos = enemy->GetPosition();

            // [PERBAIKAN] Ambil Scale musuh
            DirectX::XMFLOAT3 scale = enemy->GetScale();

            // Cari nilai scale terbesar (misal kalau X=2, Y=1, Z=2 -> ambil 2)
            float maxScale = max(scale.x, max(scale.y, scale.z));

            // Base radius 1.5f dikali scale. 
            // Kalau scale 2.0, radius jadi 3.0. Kalau scale 100, radius jadi 150.
            float cullingRadius = 150.0f * maxScale;

            // Gunakan radius dinamis
            if (!camera->CheckSphere(pos.x, pos.y, pos.z, cullingRadius))
            {
                isBodyVisible = false;
            }
        }

        if (isBodyVisible)
        {
            renderer->Draw(ShaderId::Phong, enemy->GetModel(), enemy->color);
        }

        // Projectiles tetap dirender terpisah (selalu render)
        enemy->RenderProjectiles(renderer);
    }
}

void EnemyManager::RenderDebug(ShapeRenderer* renderer)
{
    for (auto& enemy : m_enemies)
    {
        enemy->RenderDebugProjectiles(renderer);
    }
}

void EnemyManager::RespawnEnemyAs(size_t index, AttackType attack, MoveDir dir, float minX, float maxX, float minZ, float maxZ)
{
    if (index >= m_enemies.size()) return;

    auto& e = m_enemies[index];

    EnemySpawnConfig config;
    config.Position = e->GetPosition();
    config.Rotation = e->GetRotation();
    config.Color = e->color;
    config.Type = e->GetType();
    config.AttackBehavior = attack;
    config.Direction = dir;
    config.MinX = minX; config.MaxX = maxX;
    config.MinZ = minZ; config.MaxZ = maxZ;

    SpawnEnemy(config); 

    std::swap(m_enemies[index], m_enemies.back());
    m_enemies.pop_back();
}