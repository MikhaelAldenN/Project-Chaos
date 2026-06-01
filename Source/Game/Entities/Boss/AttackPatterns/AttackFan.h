#pragma once
#include "IPooledAttackPattern.h"
#include <DirectXMath.h>

// ============================================================
// AttackFan - Phase 1 attack pattern.
//
// Fires multiple waves of a spread shot (shotgun) aimed at
// the player's position at the moment of trigger. The aim
// angle is locked on trigger, so the player can dodge by
// moving after the tell.
// ============================================================

struct FanParams {
    int   lines = 5;
    int   waves = 4;
    float waveDelay = 0.133f;  // Seconds between each wave
    float spreadAngle = 0.130f;  // Radians between adjacent lines
    float speed = 35.617f;
    int   damage = 2;
    float sfxVolume = 1.0f;
};

class AttackFan : public IPooledAttackPattern {
public:
    // lockedBaseAngle: atan2(playerX - bossX, playerZ - bossZ) at trigger time
    AttackFan(const FanParams& params, float lockedBaseAngle);
    ~AttackFan() override = default;

    void StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) override;
    void Stop(NaviBoss* boss) override;

    bool IsFinished() const override;
    std::vector<Bullet*> GetActiveProjectiles() const override { return {}; }

private:
    void FireWave(NaviBoss* boss);

    FanParams                             m_params;
    float                                 m_lockedBaseAngle = 0.0f;
    std::vector<std::unique_ptr<Bullet>>* m_pool = nullptr;
    NaviBoss* m_boss = nullptr;

    bool  m_active = false;
    int   m_wavesFired = 0;
    float m_waveTimer = 0.0f;
};