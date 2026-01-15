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

    bool IsFormationActive() const { return isFormationActive; }
    int GetActiveBlockCount() const;
    const std::vector<std::unique_ptr<Block>>& GetBlocks() const { return blocks; }

private:
    // Helper grid 
    void InitPrioritySlots();
    void UpdateFormationPositions(float elapsedTime, Player* player);

    std::vector<std::unique_ptr<Block>> blocks;

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

    std::vector<DirectX::XMFLOAT3> m_sortedOffsets;

    // Helper for distance calculation
    float GetDistSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }
};