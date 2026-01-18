#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h> 
#include "Enemy.h"
#include "System/Graphics.h"

struct EnemySpawnConfig
{
    DirectX::XMFLOAT3 Position; 
    DirectX::XMFLOAT3 Rotation; 
    DirectX::XMFLOAT4 Color;    
};

namespace EnemyLevelData
{
    // Helper Colors
    static const DirectX::XMFLOAT4 Red = { 1.0f, 0.2f, 0.2f, 1.0f };
    static const DirectX::XMFLOAT4 Blue = { 0.2f, 0.2f, 1.0f, 1.0f };
    static const DirectX::XMFLOAT4 Green = { 0.2f, 1.0f, 0.2f, 1.0f };

    // The Master List of Enemies
    static const std::vector<EnemySpawnConfig> Spawns =
    {
        // Paddle 1 
        { { 10.0f, 0.0f, -28.0f }, { 0.0f, -1.6f, 0.0f }, Red },

        // Paddle 2
        { { -10.0f, 0.0f, -40.0f }, { 0.0f, 1.6f, 0.0f }, Blue },

        // Paddle 3
        { { 0.0f, 0.0f, -50.0f }, { 0.0f, 0.0f, 0.0f }, Green },

        // Paddle 4
        { { 0.0f, 0.0f, -65.5f }, { 0.0f, 0.0f, 0.0f }, Red },

        // Paddle 5
        { { 0.0f, 0.0f, -79.1f }, { 0.0f, 0.0f, 0.0f }, Blue },

        // Paddle 6
        { { 0.0f, 0.0f, -146.8f }, { 0.0f, 0.0f, 0.0f }, Green },

        // Paddle 7
        { { -6.5f, 0.0f, -151.6f }, { 0.0f, 0.0f, 0.0f }, Red },

        // Paddle 8 
        { { 6.5f, 0.0f, -151.6f }, { 0.0f, 0.0f, 0.0f }, Red },

        // Paddle 9
        { { -3.5f, 0.0f, -162.9f }, { 0.0f, 0.0f, 0.0f }, Blue },

        // Paddle 10
        { { 3.5f, 0.0f, -162.9f }, { 0.0f, 0.0f, 0.0f }, Blue },

        // Paddle 11
        { { 0.0f, 0.0f, -189.0f }, { 0.0f, 0.0f, 0.0f }, Green },

        // Paddle 12
        { { -7.5f, 0.0f, -189.0f }, { 0.0f, 0.0f, 0.0f }, Red },

        // Paddle 13
        { { 7.5f, 0.0f, -189.0f }, { 0.0f, 0.0f, 0.0f }, Blue },

        // Paddle 14
        { { -9.5f, 0.0f, -260.5f }, { 0.0f, 1.6f, 0.0f }, Blue },

        // Paddle 15
        { { -9.5f, 0.0f, -270.0f }, { 0.0f, 1.6f, 0.0f }, Blue },

        // Paddle 16
        { { -9.5f, 0.0f, -280.0f }, { 0.0f, 1.6f, 0.0f }, Red },

        // Paddle 17
        { { -9.5f, 0.0f, -289.5f }, { 0.0f, 1.6f, 0.0f }, Red },
    };
}

class EnemyManager
{
public:
    EnemyManager();
    ~EnemyManager();

    void Initialize(ID3D11Device* device);
    void Update(float elapsedTime, Camera* camera);
    void Render(ModelRenderer* renderer);
    void SpawnEnemy(const EnemySpawnConfig& config);

    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return m_enemies; }

private:
    std::vector<std::unique_ptr<Enemy>> m_enemies;
};