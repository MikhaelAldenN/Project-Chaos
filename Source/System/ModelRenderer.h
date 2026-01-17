#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Model.h"
#include "Shader.h"

enum class ShaderId
{
    Basic,
    Lambert,
    Phong,

    EnumCount
};

class ModelRenderer
{
public:
    ModelRenderer(ID3D11Device* device);
    ~ModelRenderer() {}

    void Draw(ShaderId shaderId, std::shared_ptr<Model> model, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    // ï`âÊé¿çs
    void Render(const RenderContext& rc);

private:
    struct CbScene
    {
        DirectX::XMFLOAT4X4		viewProjection;
        DirectX::XMFLOAT4		lightDirection;
        DirectX::XMFLOAT4		lightColor;
        DirectX::XMFLOAT4		cameraPosition;
    };

    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4		boneTransforms[256];
    };

    struct CbObject
    {
        DirectX::XMFLOAT4       color;
    };

    struct DrawInfo
    {
        ShaderId				shaderId;
        std::shared_ptr<Model>	model;
        DirectX::XMFLOAT4       color; 
    };

    struct TransparencyDrawInfo
    {
        ShaderId				shaderId;
        const Model::Mesh* mesh;
        float					distance;
        DirectX::XMFLOAT4       color; 
    };

    std::unique_ptr<Shader>					shaders[static_cast<int>(ShaderId::EnumCount)];
    std::vector<DrawInfo>					drawInfos;
    std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

    Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	objectConstantBuffer;
};