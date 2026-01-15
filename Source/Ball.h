#pragma once
#include "Character.h"

class Ball : public Character
{
public:
    Ball();
    ~Ball() override;

    void Update(float elapsedTime, Camera* camera) override;
    void Launch();
    void Reset();
    void SetVelocity(const DirectX::XMFLOAT3& v) { velocity = v; }
    void SetActive(bool active) { isActive = active; }
    void SetBoundariesEnabled(bool enable) { boundariesEnabled = enable; }

    CharacterMovement* GetMovement() const { return movement; }
    DirectX::XMFLOAT3 GetVelocity() const { return velocity; }

    float GetRadius() const { return radius; }
    bool IsActive() const { return isActive; }

private:
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };

    // ----------------------------------------------------
    // PHYSICS & MOVEMENT SETTINGS
    // ----------------------------------------------------
    float speed = 7.0f;             // Speed of the ball
    float radius = 0.1f;            // Collision radius 

    // Launch Settings 
    float launchDirX = 0.3f;        // Initial X direction
    float launchDirZ = 1.0f;        // Initial Z direction (Forward)

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