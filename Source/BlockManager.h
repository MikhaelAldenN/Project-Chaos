#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <functional>
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
    void UpdateShieldLogic(bool isInputHeld, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime);
    void UpdateShootLogic(bool isInputPressed, const DirectX::XMFLOAT3& mouseWorldPos, const DirectX::XMFLOAT3& playerPos, float elapsedTime);
    void Render(ModelRenderer* renderer);
    void CheckCollision(Ball* ball);
    bool CheckEnemyCollision(Ball* ball);

    // Setters
    void ActivateFormationMode() { isFormationActive = true; }
    void ActivateInvincibility(float duration) { m_isInvincible = true; m_invincibleTimer = duration; }
    void AddBlockFromItem(const DirectX::XMFLOAT3& startPos);
    void SetOnBlockHitCallback(std::function<void()> callback) { m_onBlockHitCallback = callback; }
    void TriggerBlockBreakParams() { if (m_onBlockHitCallback) m_onBlockHitCallback(); }

    // Getters
    bool IsFormationActive() const { return isFormationActive; }
    bool IsInvincible() const { return m_isInvincible; }
    bool IsShieldActive() const { return isShieldActive; }
    int GetActiveBlockCount() const;

    void SpawnAllyBlock(Player* player);
    void ClearBlocks()
    {
        blocks.clear();
        m_formationBlocks.clear();
    }

    // Accessors for CollisionManager (Adjusted for unique_ptr)
    const std::vector<std::unique_ptr<Block>>& GetBlocks() const { return blocks; }
    std::vector<std::unique_ptr<Block>>& GetBlocks() { return blocks; }

    // Color Setting
    DirectX::XMFLOAT4 globalBlockColor = { 0.96f, 0.80f, 0.23f, 1.0f };
    DirectX::XMFLOAT4 invincibleColor = { 0.275f, 0.275f, 0.275f, 1.0f };
    float BlinkSpeed = 10.0f;

    // Formation Settings
    struct FormationConfig
    {
        float wiggleMinDistSq = 2.5f;
        float wiggleAmplitude = 0.5f;
        float noiseFrequency = 3.5f;
        float noiseThreshold = 0.8f;
        float intensityScale = 1.5f;
        float oscSpeedX = 10.0f;
        float oscSpeedZ = 8.0f;
        float speedFront = 30.0f;
        float speedTrail = 20.0f;
        float speedBase = 20.0f;
        float catchUpThreshold = 0.25f;
        float catchUpMult = 0.6f;
    };
    FormationConfig m_config;

    // Shield settings
    struct ShieldConfig {
        bool  Enabled = true;
        float MaxTetherDistance = 5.0f;
        float MoveSpeed = 6.0f;
        float Spacing = 1.4f;
        float ArrivalThresholdSq = 0.1f;
        float WakeUpDistanceSq = 0.05f;
    };
    ShieldConfig shieldSettings;

    struct ShootConfig {
        float Cooldown = 0.5f;
        float ProjectileSpeed = 10.0f;
        float MaxRange = 100.0f;
    };
    ShootConfig shootSettings;

private:
    void CalculateShieldOffsets();
    void InitPrioritySlots();
    void UpdateFormationPositions(float elapsedTime, Player* player);

    // Helper for distance calculation
    float GetDistSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // STORAGE: Changed to unique_ptr to match your intended logic
    std::vector<std::unique_ptr<Block>> blocks;

    // Non-owning pointers for formation logic
    std::vector<Block*> m_formationBlocks;

    // Grid Configuration
    int m_rows = 8;
    int m_columns = 8;
    float m_xSpacing = 0.7f;
    float m_zSpacing = 0.7f;
    float m_zOffsetWorld = 2.5f;
    float m_blockHalfSize = 0.3f;
    float m_playerSpawnOffsetX = -0.5f;

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

    // Callback
    std::function<void()> m_onBlockHitCallback = nullptr;

    // Invincible State
    bool m_isInvincible = false;
    float m_invincibleTimer = 0.0f;
    DirectX::XMFLOAT4 m_currentInvincibleColor;
};