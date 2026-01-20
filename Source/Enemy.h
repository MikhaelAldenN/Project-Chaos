#pragma once

#include "Ball.h"
#include "Character.h"
#include "EnemyManager.h"
#include <cmath>
#include <deque>
#include <DirectXMath.h>
#include <memory>
#include <string> 
#include <vector>
#include "System/Model.h"

enum class EnemyType;
enum class AttackType;

class Enemy : public Character
{
public:
    Enemy(ID3D11Device* device, const char* filePath, DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 startRot, DirectX::XMFLOAT4 startColor, EnemyType type, AttackType attackType);
    ~Enemy() override;

    void Update(float elapsedTime, Camera* camera) override;
    void SetPosition(const DirectX::XMFLOAT3& pos);
    void SetRotation(const DirectX::XMFLOAT3& rot);
    void RenderProjectiles(ModelRenderer* renderer);

    DirectX::XMFLOAT3 GetPosition() const;
    DirectX::XMFLOAT3 GetRotation() const;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT3 GetOriginalPosition() const { return originalPosition; }
    DirectX::XMFLOAT3 GetOriginalRotation() const { return originalRotation; }
    
    EnemyType GetType() const { return m_type; }
    std::shared_ptr<Model> GetModel() const { return m_model; }

private:
    void UpdateAttackLogic(float elapsedTime, Camera* camera);

    DirectX::XMFLOAT3 GetForwardVector() const;
    DirectX::XMFLOAT3 originalPosition;
    DirectX::XMFLOAT3 originalRotation;
    EnemyType m_type;
    AttackType m_attackType;
    std::shared_ptr<Model> m_model;
    std::deque<std::unique_ptr<Ball>> m_projectiles;

    // ==========================================
    // ATTACK SETTINGS 
    // ==========================================
    // Gameplay Stats
    float m_attackTimer         = 0.0f;
    float m_fireRate            = 1.5f;   // Time between shots (seconds)
    float m_projectileSpeed     = 10.0f;  // Speed of the ball
    float m_activationDistance  = 50.0f;  // Enemy starts firing when player is this close
    float m_despawnDistance     = 55.0f;  // Projectiles die when this far from player

    // Constraints
    static constexpr int MAX_PROJECTILES = 5; // Maximum active bullets per enemy

    // Spawn Offsets (Where the ball appears relative to enemy)
    static constexpr float SPAWN_OFFSET_FWD = 0.6f;
    static constexpr float SPAWN_OFFSET_Y = 0.6f;

    // Visuals
    DirectX::XMFLOAT4 m_projectileColor = { 1.0f, 1.0f, 1.0f, 1.0f }; 
};