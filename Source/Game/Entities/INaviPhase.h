#pragma once
#include <d3d11.h>
#include <memory>
#include "IBossAttackPattern.h" 

class NaviBoss;
class Camera;
class Player;

class INaviPhase {
public:
    virtual ~INaviPhase() = default;

    virtual void Enter(NaviBoss* boss) = 0;
    virtual void Update(float dt, NaviBoss* boss) = 0;
    virtual void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) = 0;
    virtual void Exit(NaviBoss* boss) = 0;

    // FSM System Hooks (Virtual default agar tidak error di phase yang belum pakai FSM)
    virtual void AddAttack(std::unique_ptr<IBossAttackPattern> attack) {}
    virtual bool HasActiveAttacks() const { return false; }
    virtual Player* GetAITarget() const { return nullptr; }
};