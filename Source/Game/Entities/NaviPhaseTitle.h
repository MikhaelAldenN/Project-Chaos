#pragma once
#include "INaviPhase.h"
#include <memory>

class Sprite;
class Player;

// ============================================================
// NaviPhaseTitle
//   - Renders "Data/Sprite/SystemChaos.png" full-screen.
//   - Pressing Enter transitions to NaviPhaseNormal.
//   - This is the FIRST phase registered on NaviBoss.
// ============================================================
class NaviPhaseTitle : public INaviPhase
{
public:
    explicit NaviPhaseTitle(Player* player = nullptr);
    ~NaviPhaseTitle() override = default;

    // INaviPhase interface
    void Enter(NaviBoss* boss)                                                     override;
    void Update(float dt, NaviBoss* boss)                                           override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss)                                                     override;

private:
    Player* m_player = nullptr;
    std::unique_ptr<Sprite> m_titleSprite;

    // Fade-in state
    float m_alpha = 0.0f;
    float m_fadeInSpeed = 1.2f;   // seconds to reach full opacity
    bool  m_ready = false;   // true once fully faded in
};