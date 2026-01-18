#include "Enemy.h"

using namespace DirectX;

Enemy::Enemy(ID3D11Device* device, const char* filePath, XMFLOAT3 startPos, XMFLOAT3 startRot, XMFLOAT4 startColor)
{
    m_model = std::make_shared<Model>(device, filePath);
    model = m_model; 

    // Default Transform
    movement->SetPosition(startPos);
    movement->SetRotation(startRot);
    originalPosition = startPos;
    originalRotation = startRot;
    this->color = startColor;
}

Enemy::~Enemy(){}

void Enemy::Update(float elapsedTime, Camera* camera)
{
    if (movement) movement->Update(elapsedTime);
    SyncData();
}

void Enemy::SetPosition(const DirectX::XMFLOAT3& pos) { movement->SetPosition(pos); }
void Enemy::SetRotation(const DirectX::XMFLOAT3& rot) { movement->SetRotation(rot); }

DirectX::XMFLOAT3 Enemy::GetPosition() const { return movement->GetPosition(); }
DirectX::XMFLOAT3 Enemy::GetRotation() const { return movement->GetRotation(); }