#pragma once
#include "IPooledAttackPattern.h"
#include "Primitive.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackRain - Phase 1 attack pattern.
//
// Area-denial attack. Marks a zone with a blinking warning,
// then bombards it with falling/sweeping bullets.
//
// Supports three modes:
//   - Horizontal sweep (left or right across the arena)
//   - Vertical sweep (top-down through a lane)
//   - Dual pillar (two synchronized vertical pillars, L+R)
//
// States: 0=Off, 1=Warning, 2=Active, 3=Dissipating
// ============================================================

struct RainParams {
    float minSpeed = 50.0f;
    float maxSpeed = 90.0f;
    float warningDuration = 1.2f;
    float activeDuration = 2.0f;
    float width = 25.0f;
    float depth = 40.0f;
    float damage = 1.0f;
    float sfxVolume = 1.0f;
};

enum class RainMode {
    HorizontalSweep,  // Bullets sweep L or R across a horizontal zone
    VerticalSweep,    // Bullets fall top-down through a vertical lane
    DualPillar        // Two synchronized vertical pillar zones
};

class AttackRain : public IPooledAttackPattern {
public:
    // sweepDir: +1 = right/down, -1 = left/up (only relevant for HorizontalSweep)
    // isPositiveSide: which side of arena to place zone (for Horizontal/Vertical)
    AttackRain(const RainParams& params, RainMode mode, bool isPositiveSide, float sweepDir, Player* target);
    ~AttackRain() override = default;

    void StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) override;
    void Stop(NaviBoss* boss) override;

    bool IsFinished() const override;
    std::vector<Bullet*> GetActiveProjectiles() const override { return {}; }

    // State query — used by the phase for collision logic
    int               GetState()   const { return m_state; }
    DirectX::XMFLOAT3 GetCenter()  const { return m_center; }
    DirectX::XMFLOAT3 GetCenter2() const { return m_center2; }
    float             GetActualWidth() const;
    float             GetActualDepth() const;

private:
    void ClearVFX();

    RainParams                            m_params;
    RainMode                              m_mode;
    bool                                  m_isPositiveSide;
    float                                 m_sweepDir;

    Player* m_target = nullptr;

    std::vector<std::unique_ptr<Bullet>>* m_pool = nullptr;

    int               m_state = 0;
    float             m_timer = 0.0f;
    float             m_sfxTimer = 0.0f;
    const float       SFX_LOOP = 0.8f;

    DirectX::XMFLOAT3 m_center = {};
    DirectX::XMFLOAT3 m_center2 = {};

    std::vector<int>  m_vfxHandles;
    std::unique_ptr<Primitive> m_zonePrimitive;
};