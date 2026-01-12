#pragma once
#include "Character.h"
#include <DirectXMath.h>

class Block : public Character
{
public:
    Block();
    ~Block() override;

    void Update(float elapsedTime, Camera* camera) override;
    void OnHit();
    bool IsActive() const { return isActive; }

    CharacterMovement* GetMovement() const { return movement; }

private:
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Block Model Size

    bool isActive = true;
};