#pragma once
#include "IPooledAttackPattern.h"
#include <DirectXMath.h>

// ============================================================
// Attack_Radial - Phase 1 attack pattern.
//
// Fires three interlocked radial bursts in quick succession.
// Each burst fires N bullets in a full circle. Odd-numbered
// bursts are offset by half a step to create a denser pattern.
// ============================================================

struct RadialParams {
    int   count = 55;
    float speed = 22.14f;
    float burstDelay = 0.156f;  // Seconds between each of the 3 bursts
    int   burstCount = 3;       // Total bursts per trigger
    int   damage = 1;
    float sfxVolume = 1.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 0.2f, 0.2f, 1.0f };
};

class Attack_Radial : public IPooledAttackPattern {
public:
    explicit Attack_Radial(const RadialParams& params);
    ~Attack_Radial() override = default;

    void StartPooled(NaviBoss* boss, std::vector<std::unique_ptr<Bullet>>* pool) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, NaviBoss* boss) override;
    void Stop(NaviBoss* boss) override;

    bool IsFinished() const override;
    std::vector<Bullet*> GetActiveProjectiles() const override { return {}; }

private:
    void FireBurst(NaviBoss* boss, float angleOffset);

    RadialParams                          m_params;
    std::vector<std::unique_ptr<Bullet>>* m_pool = nullptr;
    NaviBoss* m_boss = nullptr;

    bool  m_active = false;
    int   m_burstsFired = 0;
    float m_burstTimer = 0.0f;
};