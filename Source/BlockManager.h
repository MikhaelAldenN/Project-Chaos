#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "Ball.h"
#include "Block.h"
#include "Camera.h"
#include "Player.h"
#include "System/ModelRenderer.h"
#include <functional>

class BlockManager
{
public:
    BlockManager();
    ~BlockManager();

    void Initialize(Player* player);
    void Update(float elapsedTime, Camera* camera, Player* player);
    void UpdateShieldLogic(bool isInputHeld, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime);
    void UpdateShootLogic(bool isInputPressed, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime);
    void Render(ModelRenderer* renderer);
    void CheckCollision(Ball* ball);
    bool CheckEnemyCollision(Ball* ball);
    void ActivateFormationMode() { isFormationActive = true; }
    void SetOnBlockHitCallback(std::function<void()> callback) { m_onBlockHitCallback = callback; }
    bool IsFormationActive() const { return isFormationActive; }
    bool IsShieldActive() const { return isShieldActive; }
    int GetActiveBlockCount() const;
    const std::vector<Block>& GetBlocks() const { return blocks; }
    std::vector<Block>& GetBlocks() { return blocks; }

    // Color Setting
    DirectX::XMFLOAT4 globalBlockColor = { 0.96f, 0.80f, 0.23f, 1.0f };

    // Formation Settings
    struct FormationConfig
    {
        // Random Wiggle Settings 
        float wiggleMinDistSq   = 2.5f;     // Blocks closer than this won't wiggle (1.5m^2)
        float wiggleAmplitude   = 0.5f;     // How far they move (Distance)
        float noiseFrequency    = 3.5f;     // How fast the "who moves" cycle changes
        float noiseThreshold    = 0.8f;     // Higher = fewer blocks move at once
        float intensityScale    = 1.5f;     // Strength multiplier
        float oscSpeedX         = 10.0f;    // Shake speed X
        float oscSpeedZ         = 8.0f;     // Shake speed Z

        // Movement Smoothing Settings 
        float speedFront        = 30.0f;    // Speed when clearing path (Front)
        float speedTrail        = 20.0f;    // Speed when following (Back)
        float speedBase         = 20.0f;    // Speed for side blocks
        float catchUpThreshold  = 0.25f;    // If lagging behind > 0.5m, boost speed
        float catchUpMult       = 0.6f;     // Speed multiplier when lagging
    };
    FormationConfig m_config;

    // Shield settings
    struct ShieldConfig {
        bool  Enabled = true;
        float MaxTetherDistance = 5.0f;     // How far from player the shield can go
        float MoveSpeed = 6.0f;             // Lerp speed for blocks
        float Spacing = 1.4f;               // Gap between shield blocks
        float ArrivalThresholdSq = 0.1f;    // Distance squared to consider "Arrived" (for collision)
        float WakeUpDistanceSq = 0.05f;     // Min movement to wake up a block
    };
    ShieldConfig shieldSettings;

    struct ShootConfig {
        float Cooldown = 0.5f;              // Time between shots
        float ProjectileSpeed = 10.0f;      // Speed of the block
        float MaxRange = 100.0f;            // Max distance before auto-destroy 
    };
    ShootConfig shootSettings;

private:
    // Helper grid 
    void CalculateShieldOffsets();
    void InitPrioritySlots();
    void UpdateFormationPositions(float elapsedTime, Player* player);

    std::vector<Block> blocks;
    std::vector<Block*> m_formationBlocks;

    // Grid Configuration
    int m_rows = 8;                         // Row Blocks
    int m_columns = 8;                      // Column Blocks
    float m_xSpacing = 0.7f;                // Space between blocks X
    float m_zSpacing = 0.7f;                // Space between blocks Z
    float m_zOffsetWorld = 2.5f;            // Start Z position in World

    // Collision Setting (Physics Size)
    float m_blockHalfSize = 0.3f;           // Hitbox size (Half-extent)

    // Player Spawn Adjustments
    float m_playerSpawnOffsetX = -0.5f;     // X Value Player

    // Formation State
    bool isFormationActive = false;
    float formationSpacing = 1.0f;
    float m_formationTime = 0.0f;

    // Shield State
    bool isShieldActive = false;
    bool wasShieldActive = false;
    std::vector<DirectX::XMFLOAT3> m_shieldOffsets;
    std::vector<Block*> m_shieldAssignments;

    std::vector<DirectX::XMFLOAT3> m_sortedOffsets;

    // Shoot State
    float m_shootTimer = 0.0f;
    bool m_wasShootPressed = false;

    // Helper for distance calculation
    float GetDistSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

	// Callback when a block is hit
    std::function<void()> m_onBlockHitCallback = nullptr;
};