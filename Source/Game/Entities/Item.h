#pragma once
#include "Character.h"
#include <DirectXMath.h>

enum class ItemType { Heal, Invincible };

class Item : public Character
{
public:
    Item(ID3D11Device* device, const DirectX::XMFLOAT3& position, ItemType type);
    ~Item() override = default;

    void Update(float elapsedTime, Camera* camera) override;
    void Render(ModelRenderer* renderer);

    // Getters
    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }
    void SetType(ItemType type) 
    {
        m_type = type;
        if (m_type == ItemType::Heal) 
        {
            color = { 1.0f, 0.89f, 0.58f, 1.0f }; 
        }
        else if (m_type == ItemType::Invincible) 
        {
            color = { 0.5f, 0.5f, 0.5f, 1.0f };   
        }
    }
    ItemType GetType() const { return m_type; }
    DirectX::XMFLOAT3 GetPosition() const { return movement->GetPosition(); }
    DirectX::XMFLOAT3 GetRotation() const { return movement->GetRotation(); }
    DirectX::XMFLOAT3 GetBasePosition() const 
    {
        DirectX::XMFLOAT3 pos = movement->GetPosition();
        pos.y = originalY; 
        return pos;
    }
    void ResetAnimation() { animTime = 0.0f; }
    void SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); originalY = pos.y; }
    void SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

    // Public for GUI
    DirectX::XMFLOAT4 color = { 1.0f, 0.89f, 0.58f, 1.0f }; // Pale Yellow (Enemy Ball Color)
    DirectX::XMFLOAT3 scale = { 2.0f, 2.0f, 2.0f };         // Smaller than Block (3.0)

private:
    ItemType m_type;
    bool isActive = true;

    // Animation State
    float originalY = 0.0f;
    float animTime = 0.0f;

    // Animation Config
    const float FLOAT_SPEED = 2.0f;
    const float FLOAT_AMP = 0.5f;
    const float SPIN_SPEED = 1.5f;
};