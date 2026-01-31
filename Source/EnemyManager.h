#pragma once

#include <DirectXMath.h> 
#include <memory>
#include <vector>
#include "System/Graphics.h"

class Enemy;
class ShapeRenderer;

enum class EnemyType
{
    Paddle,
    Ball
};

enum class AttackType
{
    None,
    Static,   
    Tracking,
    TrackingHorizontal,
    TrackingRandom
};

enum class MoveDir
{
    None,
    Left,
    Right
};

struct EnemySpawnConfig
{
    DirectX::XMFLOAT3 Position; 
    DirectX::XMFLOAT3 Rotation; 
    DirectX::XMFLOAT4 Color;    
    EnemyType Type = EnemyType::Paddle;
    AttackType AttackBehavior = AttackType::None;
    MoveDir Direction = MoveDir::None;
    float MinX = 0.0f;
    float MaxX = 0.0f;
    float MinZ = 0.0f; 
    float MaxZ = 0.0f; 
};

namespace EnemyLevelData
{
    // ==========================================
    // COLOR PRESETS
    // ==========================================
    static const DirectX::XMFLOAT4 Blue         = { 0.0f, 0.0f, 0.8f, 1.0f };
    static const DirectX::XMFLOAT4 PaleYellow   = { 0.76f, 0.74f, 0.56f, 1.0f };

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
        // ------------------------------------------
        // PADDLE LIST
        // ------------------------------------------
        // Paddle 1
        { { 4.6f, 0.0f, -40.3f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 2
        { { -4.7f, 0.0f, -52.9f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 3
        { { -2.9f, 0.0f, -157.2f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Tracking },
        // Paddle 4
        { { 1.3f, 0.0f, -192.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Left, -4.4f, 4.3f },
        // Paddle 5
        { { 28.1f, 0.0f, -226.9f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 6
        { { 28.1f, 0.0f, -230.0f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 7
        { { 28.1f, 0.0f, -233.2f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 8
        { { 28.1f, 0.0f, -236.3f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 9
        { { 28.1f, 0.0f, -239.4f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 10
        { { 28.1f, 0.0f, -242.5f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 11
        { { 28.1f, 0.0f, -245.7f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 12
        { { 28.1f, 0.0f, -249.1f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },

        // ------------------------------------------
        // BALL LIST
        // ------------------------------------------
        // Ball 1
        
        
    };
}

class EnemyManager
{
public:
    EnemyManager();
    ~EnemyManager();

    void Initialize(ID3D11Device* device);
    void Update(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos);
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void RenderDebug(ShapeRenderer* renderer);
    void RespawnEnemyAs(size_t index, AttackType attack, MoveDir dir = MoveDir::None, float minX = 0, float maxX = 0, float minZ = 0, float maxZ = 0);
    void SpawnEnemy(const EnemySpawnConfig& config);

    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return m_enemies; }

private:
    std::vector<std::unique_ptr<Enemy>> m_enemies;
};