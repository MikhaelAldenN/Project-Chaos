#pragma once

#include <DirectXMath.h> 
#include <memory>
#include <vector>
#include "System/Graphics.h"

class Enemy;

enum class EnemyType
{
    Paddle,
    Ball
};

enum class AttackType
{
    None,
    Static,   
    Tracking  
};

struct EnemySpawnConfig
{
    DirectX::XMFLOAT3 Position; 
    DirectX::XMFLOAT3 Rotation; 
    DirectX::XMFLOAT4 Color;    
    EnemyType Type = EnemyType::Paddle;
    AttackType AttackBehavior = AttackType::None;
};

namespace EnemyLevelData
{
    // ==========================================
    // COLOR PRESETS
    // ==========================================
    static const DirectX::XMFLOAT4 Blue         = { 0.0f, 0.0f, 0.8f, 1.0f };
    static const DirectX::XMFLOAT4 Yellow       = { 0.96f, 0.80f, 0.23f, 1.0f };

    // ==========================================
    // ROTATION PRESETS
    // ==========================================
    namespace Rot
    {
        static const DirectX::XMFLOAT3 Backward = { 0.0f, 0.0f, 0.0f };
        static const DirectX::XMFLOAT3 Forward  = { 0.0f, DirectX::XM_PI, 0.0f };
        static const DirectX::XMFLOAT3 Left     = { 0.0f, DirectX::XM_PIDIV2, 0.0f };
        static const DirectX::XMFLOAT3 Right    = { 0.0f, -DirectX::XM_PIDIV2, 0.0f };
    }

    // ==========================================
    // MASTER SPAWN LIST
    // ==========================================
    static const std::vector<EnemySpawnConfig> Spawns =
    {
        // Paddle 1 
        { { 10.0f, 0.0f, -28.0f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 2
        { { -10.0f, 0.0f, -40.0f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 3
        { { 0.0f, 0.0f, -50.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 4
        { { 0.0f, 0.0f, -65.5f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 5
        { { 0.0f, 0.0f, -79.1f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 6
        { { 0.0f, 0.0f, -146.8f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 7
        { { -6.5f, 0.0f, -151.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 8 
        { { 6.5f, 0.0f, -151.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 9
        { { -3.5f, 0.0f, -162.9f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 10
        { { 3.5f, 0.0f, -162.9f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 11
        { { 0.0f, 0.0f, -189.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 12
        { { -7.5f, 0.0f, -189.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 13
        { { 7.5f, 0.0f, -189.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // Paddle 14
        { { -9.5f, 0.0f, -260.5f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 15
        { { -9.5f, 0.0f, -270.0f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 16
        { { -9.5f, 0.0f, -280.0f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },

        // Paddle 17
        { { -9.5f, 0.0f, -289.5f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },
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