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
    if (config.Type == EnemyType::Ball)
    {
        modelPath = "Data/Model/Character/PLACEHOLDER_mdl_Ball.glb";
    }
    else
    {
        modelPath = "Data/Model/Character/PLACEHOLDER_mdl_Paddle.glb";
    }

    auto newEnemy = std::make_unique<Enemy>(
        device,
        modelPath,       
        config.Position,
        config.Rotation,
        config.Color,
        config.Type      
    );
    m_enemies.push_back(std::move(newEnemy));
}

void EnemyManager::Update(float elapsedTime, Camera* camera)
{
    for (auto& enemy : m_enemies)
    {
        enemy->Update(elapsedTime, camera);
    }
}

void EnemyManager::Render(ModelRenderer* renderer)
{
    for (auto& enemy : m_enemies)
    {
        renderer->Draw(
            ShaderId::Phong,
            enemy->GetModel(),
            enemy->color
        );
    }
}