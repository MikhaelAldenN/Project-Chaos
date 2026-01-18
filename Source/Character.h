#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include "System/ModelRenderer.h"
#include "System/ShapeRenderer.h"
#include "System/Model.h"
#include "CharacterMovement.h" 

class Camera; // Forward declaration

class Character
{
public:
    Character();
    virtual ~Character();
    virtual void Update(float elapsedTime, Camera* camera) = 0;

    // Standard Render Pipeline
    void Render(ModelRenderer* renderer);
    void RenderDebug(const RenderContext& rc, ShapeRenderer* renderer);

    DirectX::XMFLOAT3 GetPosition() const { return movement->GetPosition(); }
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

protected:
    // Syncs physics data to the visual model's root node
    void SyncData();

protected:
    // Components
    CharacterMovement* movement;
    std::shared_ptr<Model> model;
};