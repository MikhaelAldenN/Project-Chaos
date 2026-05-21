#pragma once

#include "Character.h"
#include "Bullet.h"
#include <DirectXMath.h>
#include <memory>
#include <deque>

// Forward Declarations for fast compilation
class Player;
class EnemyManager;
class Camera;
class ModelRenderer;

class NaviAlly : public Character
{
public:
    explicit NaviAlly(ID3D11Device* device, const Player* targetPlayer, EnemyManager* enemyManager);
    ~NaviAlly() override = default;

    void Update(float elapsedTime, Camera* camera) override;
    void Render(ModelRenderer* renderer);

    void RenderProjectiles(ModelRenderer* renderer);
    std::deque<std::unique_ptr<Bullet>>& GetProjectiles() { return m_projectiles; }

    void SetPosition(const DirectX::XMFLOAT3& pos);

private:
    void UpdateHoverLogic(float elapsedTime);
    void UpdateShootingLogic(float elapsedTime, Camera* camera);
    void UpdateProjectiles(float elapsedTime, Camera* camera);
    void FireAtTarget(const DirectX::XMFLOAT3& targetPos);

    // Safe Observer Pointers
    const Player* m_targetPlayer{ nullptr };
    EnemyManager* m_enemyManager{ nullptr };

    // State Variables 
    float m_animTime{ 0.0f };
    float m_fireTimer{ 0.0f };
    DirectX::XMFLOAT4 m_color{ 0.0f, 0.8f, 1.0f, 1.0f };

    // True Object Pool for Navi's Bullets
    std::deque<std::unique_ptr<Bullet>> m_projectiles{};

    // ====================================================
    // COMPILE-TIME CONSTANTS (Zero Memory Cost)
    // ====================================================
    static constexpr float FLOAT_SPEED{ 2.0f };
    static constexpr float FLOAT_AMP{ 0.25f };
    static constexpr float FOLLOW_SPEED{ 15.0f };
    static constexpr float HOVER_HEIGHT{ 2.0f };

    static constexpr float FIRE_RATE{ 0.5f }; 
    static constexpr float ATTACK_RANGE_SQ{ 900.0f }; 

    static constexpr int   MAX_BULLETS{ 10 };
    static constexpr float BULLET_SPEED{ 25.0f };
    static constexpr float DESPAWN_DIST_SQ{ 2500.0f }; 
};