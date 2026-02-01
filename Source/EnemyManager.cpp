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

    m_enemies.push_back(std::move(newEnemy));
}

void EnemyManager::Update(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos)
{
    for (auto& enemy : m_enemies)
    {
        enemy->Update(elapsedTime, camera);
        enemy->UpdateTracking(elapsedTime, camera, playerPos);
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