#pragma once

#include "Character.h"
#include <DirectXMath.h>
#include <memory>
#include <string> 
#include "System/Model.h"

enum class EnemyType;

class Enemy : public Character
{
public:
    Enemy(ID3D11Device* device, const char* filePath, DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 startRot, DirectX::XMFLOAT4 startColor, EnemyType type);
    ~Enemy() override;

    void Update(float elapsedTime, Camera* camera) override;
    void SetPosition(const DirectX::XMFLOAT3& pos);
    void SetRotation(const DirectX::XMFLOAT3& rot);

    DirectX::XMFLOAT3 GetPosition() const;
    DirectX::XMFLOAT3 GetRotation() const;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT3 GetOriginalPosition() const { return originalPosition; }
    DirectX::XMFLOAT3 GetOriginalRotation() const { return originalRotation; }
    
    EnemyType GetType() const { return m_type; }
    std::shared_ptr<Model> GetModel() const { return m_model; }

private:
    DirectX::XMFLOAT3 originalPosition;
    DirectX::XMFLOAT3 originalRotation;
    EnemyType m_type;
    std::shared_ptr<Model> m_model;
};