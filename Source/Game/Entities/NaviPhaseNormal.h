#pragma once
#include "INaviPhase.h"

class NaviPhaseNormal : public INaviPhase {
public:
    NaviPhaseNormal() = default;
    ~NaviPhaseNormal() override = default;

    // --- Kontrak Wajib dari INaviPhase ---
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

private:
    // Timer sementara untuk keperluan testing
    float m_phaseTimer = 0.0f;
};