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

    CharacterMovement* GetMovement() const { return movement; }

private:
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Block Model Size

    bool isActive = true;

    // --- Internal State Variable ---
    bool isRelocating = false;
};