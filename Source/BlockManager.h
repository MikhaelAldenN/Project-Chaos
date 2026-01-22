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
    void Render(ModelRenderer* renderer);
    void CheckCollision(Ball* ball);
    void ActivateFormationMode() { isFormationActive = true; }
    void SetOnBlockHitCallback(std::function<void()> callback) { m_onBlockHitCallback = callback; }
    bool IsFormationActive() const { return isFormationActive; }
    int GetActiveBlockCount() const;

    void SpawnAllyBlock(Player* player);
    void ClearBlocks()
    {
        blocks.clear();
        m_formationBlocks.clear(); // <--- TAMBAHKAN INI
    }

    const std::vector<std::unique_ptr<Block>>& GetBlocks() const { return blocks; }

    // Color Setting
    DirectX::XMFLOAT4 globalBlockColor = { 0.96f, 0.80f, 0.23f, 1.0f };

    // Formation Settings
    struct FormationConfig
    {
        // Random Wiggle Settings 
        float wiggleMinDistSq   = 2.5f;   // Blocks closer than this won't wiggle (1.5m^2)
        float wiggleAmplitude   = 0.5f;   // How far they move (Distance)
        float noiseFrequency    = 3.5f;   // How fast the "who moves" cycle changes
        float noiseThreshold    = 0.8f;   // Higher = fewer blocks move at once
        float intensityScale    = 1.5f;   // Strength multiplier
        float oscSpeedX         = 10.0f;  // Shake speed X
        float oscSpeedZ         = 8.0f;   // Shake speed Z

        // Movement Smoothing Settings 
        float speedFront        = 30.0f;  // Speed when clearing path (Front)
        float speedTrail        = 20.0f;  // Speed when following (Back)
        float speedBase         = 20.0f;  // Speed for side blocks
        float catchUpThreshold  = 0.25f;  // If lagging behind > 0.5m, boost speed
        float catchUpMult       = 0.6f;   // Speed multiplier when lagging
    };
    FormationConfig m_config;

private:
    // Helper grid 
    void InitPrioritySlots();
    void UpdateFormationPositions(float elapsedTime, Player* player);

    std::vector<std::unique_ptr<Block>> blocks;
    std::vector<Block*> m_formationBlocks;

    // Grid Configuration
    int m_rows = 7;                         // Row Blocks
    int m_columns = 7;                      // Column Blocks
    float m_xSpacing = 0.7f;                // Space between blocks X
    float m_zSpacing = 0.7f;                // Space between blocks Z
    float m_zOffsetWorld = 2.5f;            // Start Z position in World

    // Collision Setting (Physics Size)
    float m_blockHalfSize = 0.3f;           // Hitbox size (Half-extent)

    // Player Spawn Adjustments
    float m_playerSpawnOffsetX = -0.5f;     // X Value Player

    // --------------------------------------------------------
    // FORMATION SETTINGS
    // --------------------------------------------------------
    bool isFormationActive = false;
    float formationSpacing = 1.0f;
    float m_formationTime = 0.0f;

    std::vector<DirectX::XMFLOAT3> m_sortedOffsets;

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