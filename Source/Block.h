#pragma once
#include "Character.h"
#include <DirectXMath.h>

class Block : public Character
{
public:
    Block();
    ~Block() override;

    void Update(float elapsedTime, Camera* camera) override;
    void Render(ModelRenderer* renderer, const DirectX::XMFLOAT4& color);
    void OnHit();

    // --- SLEEP OPTIMIZATION SYSTEM ---
    void TrySleep(float elapsedTime);
    void WakeUp();
    bool IsSleeping() const { return isSleeping; }

    // --- Relocation State Accessors ---
    void SetRelocating(bool state) { isRelocating = state; if (state) WakeUp(); } // Auto-wake if moving
    bool IsRelocating() const { return isRelocating; }
    bool IsActive() const { return isActive; }

    // --- Collision State Management ---
    void ResetCollisionState() { isHittingWall = false; isStacked = false; wallNormal = { 0,0,0 }; }
    void SetWallCollision(const DirectX::XMFLOAT3& normal) { isHittingWall = true; isStacked = true; wallNormal = normal; }
    void SetStacked(bool state) { isStacked = state; }
    bool IsHittingWall() const { return isHittingWall; }
    bool IsStacked() const { return isStacked; }
    const DirectX::XMFLOAT3& GetWallNormal() const { return wallNormal; }

    // --- Filling State Management ---
    void SetFilling(bool state) { isFilling = state; if (state) WakeUp(); } // Auto-wake if moving
    bool IsFilling() const { return isFilling; }

    // --- Falling Logic ---
    void SetFalling(bool state) {
        isFalling = state;
        if (state) {
            WakeUp(); 
            movement->SetGravityEnabled(true); 
        }
    }
    bool IsFalling() const { return isFalling; }

    CharacterMovement* GetMovement() const { return movement; }

private:
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Block Model Size

    // --- Internal State Variable ---
    bool isActive = true;
    bool isFalling = false;
    bool isFilling = false;
    bool isHittingWall = false;
    bool isRelocating = false;
    bool isSleeping = false;
    bool isStacked = false;
    float sleepTimer = 0.0f;

    // Constants
    const float SLEEP_VEL_SQ = 0.05f * 0.05f; 
    const float SLEEP_TIME_REQ = 0.5f;        // Time (seconds) required to trigger sleep

    DirectX::XMFLOAT3 wallNormal = { 0,0,0 };
};