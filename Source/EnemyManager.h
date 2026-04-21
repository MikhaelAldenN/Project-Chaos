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
    Ball,
    Pentagon 
};

enum class AttackType
{
    None,
    Static,
    Tracking,
    TrackingHorizontal,
    TrackingRandom,
    RadialBurst 
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

    DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
    float BaseSpeed = 2.0f;         // Game Beyond Enemy Speed
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
        static const DirectX::XMFLOAT3 Forward  = { 0.0f, 180.0f, 0.0f };
        static const DirectX::XMFLOAT3 Left     = { 0.0f, 90.0f, 0.0f };
        static const DirectX::XMFLOAT3 Right    = { 0.0f, -90.0f, 0.0f };
    }

    // ==========================================
    // MASTER SPAWN LIST
    // ==========================================
    static const std::vector<EnemySpawnConfig> Spawns =
    {
        // 1. NONE: Just stands there, doesn't shoot or move.
        { { -16.0f, 0.0f, 15.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::None },

        // 2. STATIC: Stands completely still, but shoots at the player.
        { { -8.0f, 0.0f, 15.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },

        // 3. TRACKING: Slowly chases the player around the map and shoots.
        { { 0.0f, 0.0f, 15.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Tracking },

        // 4. TRACKING HORIZONTAL: Patrols left and right (offset -4 to +4) and shoots.
        { { 8.0f, 0.0f, 15.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Left, -4.0f, 4.0f },

        // 5. TRACKING RANDOM: Wanders randomly inside a specific box area and shoots.
        { { 16.0f, 0.0f, 15.0f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingRandom, MoveDir::None, -4.0f, 4.0f, -4.0f, 4.0f }

    };
}

class EnemyManager
{
public:
    EnemyManager();
    ~EnemyManager();

    void Initialize(ID3D11Device* device);
    void Update(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos, bool allowAttack = true);
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void RenderDebug(ShapeRenderer* renderer);
    void RespawnEnemyAs(size_t index, AttackType attack, MoveDir dir = MoveDir::None, float minX = 0, float maxX = 0, float minZ = 0, float maxZ = 0);
    void SpawnEnemy(const EnemySpawnConfig& config);

    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return m_enemies; }

private:
    std::vector<std::unique_ptr<Enemy>> m_enemies;
};