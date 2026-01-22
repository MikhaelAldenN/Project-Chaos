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

    // --- Relocation State Accessors ---
    void SetRelocating(bool state) { isRelocating = state; }
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
    void SetFilling(bool state) { isFilling = state; }
    bool IsFilling() const { return isFilling; }

    CharacterMovement* GetMovement() const { return movement; }

private:
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Block Model Size

    // --- Internal State Variable ---
    bool isActive = true;
    bool isFilling = false;
    bool isRelocating = false;
    bool isHittingWall = false;
    bool isStacked = false;
    DirectX::XMFLOAT3 wallNormal = { 0,0,0 };
};