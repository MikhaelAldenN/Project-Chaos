#pragma once
#include "Character.h"
#include <memory>        
#include "System/Model.h"

class Ball : public Character
{
public:
    Ball();
    ~Ball() override;

    void Update(float elapsedTime, Camera* camera) override;
    void Launch();
    void Fire(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& direction, float speed);
    void Reset();
    void SetVelocity(const DirectX::XMFLOAT3& v) { velocity = v; }
    void SetActive(bool active) { isActive = active; }
    void SetBoundariesEnabled(bool enable) { boundariesEnabled = enable; }

    CharacterMovement* GetMovement() const { return movement; }
    DirectX::XMFLOAT3 GetVelocity() const { return velocity; }
    DirectX::XMFLOAT3 GetPreviousPosition() const { return prevPosition; }

    std::shared_ptr<Model> GetModel() const { return model; }

    float GetRadius() const { return radius; }
    bool IsActive() const { return isActive; }

private:
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    DirectX::XMFLOAT3 prevPosition = { 0, 0, 0 };

    // ----------------------------------------------------
    // PHYSICS & MOVEMENT SETTINGS
    // ----------------------------------------------------
    float speed = 7.0f;             // Speed of the ball
    float radius = 0.25f;           // Collision radius 

    // Launch Settings 
    float launchMinX = 0.5f;        // Minimum angle (Prevents going straight)
    float launchMaxX = 1.5f;        // Maximum angle (Prevents going too sideways)

    // ----------------------------------------------------
    // ARENA BOUNDARIES
    // ----------------------------------------------------
    float xLimitLeft = -9.6f;       // Left Wall collision point
    float xLimitRight = 9.6f;       // Right Wall collision point
    float zLimitTop = 6.3f;         // Top Wall
    float zLimitBottom = -5.2f;     // Bottom (Game Over trigger)

    // ----------------------------------------------------
    // INTERNAL STATE
    // ----------------------------------------------------
    bool isActive = false;
    bool boundariesEnabled = true;
};