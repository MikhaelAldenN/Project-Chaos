#pragma once

#include <cmath>
#include <memory>        
#include <DirectXMath.h>
#include "System/Graphics.h"
#include "System/Model.h"
#include "Character.h"

class Bullet : public Character
{
public:
    Bullet();
    ~Bullet() override = default;

    void Update(float elapsedTime, Camera* camera) override;

    // Fires the bullet in a specific direction
    void Fire(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& direction, float speed);

    // Used when the bullet bounces off a wall
    void ApplyMovement(const DirectX::XMFLOAT3& newPos, const DirectX::XMFLOAT3& newVel);

    // Getters
    CharacterMovement* GetMovement() const { return movement; }
    DirectX::XMFLOAT3 GetVelocity() const { return velocity; }
    std::shared_ptr<Model> GetModel() const { return model; }
    float GetRadius() const { return radius; }
    bool IsActive() const { return isActive; }

private:
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    float radius = 0.25f;
    bool isActive = false;
};