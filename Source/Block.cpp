#include "Block.h"
#include "System/Graphics.h"

using namespace DirectX;

Block::Block()
{
    auto device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");
    scale = defaultScale;
}

Block::~Block()
{
}

void Block::Update(float elapsedTime, Camera* camera)
{
    if (!isActive) return;
    SyncData();
}

void Block::Render(ModelRenderer* renderer, const DirectX::XMFLOAT4& color)
{
    if (!isActive) return;
    renderer->Draw(ShaderId::Phong, model, color);
}

void Block::OnHit()
{
    isActive = false;
}