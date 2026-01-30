#include <algorithm>
#include "Misc.h"
#include "GpuResourceUtils.h"
#include "ModelRenderer.h"
#include "BasicShader.h"
#include "LambertShader.h"
#include "PhongShader.h"

// コンストラクタ
ModelRenderer::ModelRenderer(ID3D11Device* device)
{
    // シーン用定数バッファ
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbScene),
        sceneConstantBuffer.GetAddressOf());

    // スケルトン用定数バッファ
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbSkeleton),
        skeletonConstantBuffer.GetAddressOf());

    // Create the Object Constant Buffer
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbObject),
        objectConstantBuffer.GetAddressOf());
    
    drawInfos.reserve(2000);
    transparencyDrawInfos.reserve(2000);

    // シェーダー生成
    shaders[static_cast<int>(ShaderId::Basic)] = std::make_unique<BasicShader>(device);
    shaders[static_cast<int>(ShaderId::Lambert)] = std::make_unique<LambertShader>(device);
    shaders[static_cast<int>(ShaderId::Phong)] = std::make_unique<PhongShader>(device);
}

// Store the color in DrawInfo (Standard)
void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<Model> model, const DirectX::XMFLOAT4& color)
{
    DrawInfo& drawInfo = drawInfos.emplace_back();
    drawInfo.shaderId = shaderId;
    drawInfo.model = model;
    drawInfo.color = color;
    drawInfo.useManualMatrix = false; // Default: Pakai transform model
}

// [BARU] Overload dengan Manual Matrix
void ModelRenderer::Draw(ShaderId shader, std::shared_ptr<Model> model, DirectX::XMFLOAT4 color, const DirectX::XMFLOAT4X4& worldMatrix)
{
    DrawInfo& drawInfo = drawInfos.emplace_back();
    drawInfo.shaderId = shader;
    drawInfo.model = model;
    drawInfo.color = color;

    // Simpan Matrix Manual
    drawInfo.useManualMatrix = true;
    drawInfo.worldMatrix = worldMatrix;
}

// 描画実行
void ModelRenderer::Render(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // シーン用定数バッファ更新
    {
        static LightManager defaultLightManager;
        const LightManager* lightManager = rc.lightManager ? rc.lightManager : &defaultLightManager;

        CbScene cbScene{};
        DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
        DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
        DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);
        const DirectionalLight& directionalLight = lightManager->GetDirectionalLight();
        cbScene.lightDirection.x = directionalLight.direction.x;
        cbScene.lightDirection.y = directionalLight.direction.y;
        cbScene.lightDirection.z = directionalLight.direction.z;
        cbScene.lightColor.x = directionalLight.color.x;
        cbScene.lightColor.y = directionalLight.color.y;
        cbScene.lightColor.z = directionalLight.color.z;
        const DirectX::XMFLOAT3& eye = rc.camera->GetPosition();
        cbScene.cameraPosition.x = eye.x;
        cbScene.cameraPosition.y = eye.y;
        cbScene.cameraPosition.z = eye.z;
        dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
    }

    // 定数バッファ設定
    ID3D11Buffer* vsConstantBuffers[] =
    {
        skeletonConstantBuffer.Get(),
        sceneConstantBuffer.Get(),
    };
    ID3D11Buffer* psConstantBuffers[] =
    {
        sceneConstantBuffer.Get(),
    };
    dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
    dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

    // Bind the Object Constant Buffer to Pixel Shader Slot 2 
    dc->PSSetConstantBuffers(2, 1, objectConstantBuffer.GetAddressOf());

    // サンプラステート設定
    ID3D11SamplerState* samplerStates[] =
    {
        rc.renderState->GetSamplerState(SamplerState::LinearWrap)
    };
    dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

    // レンダーステート設定
    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

    // --------------------------------------------------------------------------------
    // [MODIFIKASI] Lambda drawMesh sekarang menerima Override Matrix
    // --------------------------------------------------------------------------------
    auto drawMesh = [&](const Model::Mesh& mesh, Shader* shader, bool useManual, const DirectX::XMFLOAT4X4& manualMatrix)
        {
            // 頂点バッファ設定
            UINT stride = sizeof(Model::Vertex);
            UINT offset = 0;
            dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
            dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // スケルトン用定数バッファ更新
            CbSkeleton cbSkeleton{};

            // Logic Matrix Override
            if (mesh.bones.size() > 0)
            {
                for (size_t i = 0; i < mesh.bones.size(); ++i)
                {
                    const Model::Bone& bone = mesh.bones.at(i);

                    // [MODIFIKASI] Pilih Matrix (Manual atau Node Internal)
                    DirectX::XMMATRIX WorldTransform;
                    if (useManual) {
                        WorldTransform = DirectX::XMLoadFloat4x4(&manualMatrix);
                    }
                    else {
                        WorldTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
                    }

                    DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
                    DirectX::XMMATRIX BoneTransform = OffsetTransform * WorldTransform;
                    DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
                }
            }
            else
            {
                // Static Mesh (Tanpa Bone)
                // [MODIFIKASI] Gunakan Manual Matrix jika aktif
                if (useManual) {
                    cbSkeleton.boneTransforms[0] = manualMatrix;
                }
                else {
                    cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
                }
            }
            dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

            // 更新
            shader->Update(rc, mesh);

            // 描画
            dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
        };
    // --------------------------------------------------------------------------------

    DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetPosition());
    DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.camera->GetFront());

    // ブレンドステート設定
    dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // 不透明描画処理
    for (DrawInfo& drawInfo : drawInfos)
    {
        Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
        shader->Begin(rc);

        CbObject cbObject;
        cbObject.color = drawInfo.color;
        dc->UpdateSubresource(objectConstantBuffer.Get(), 0, 0, &cbObject, 0, 0);

        for (const Model::Mesh& mesh : drawInfo.model->GetMeshes())
        {
            // 半透明メッシュ登録
            if (mesh.material->alphaMode == Model::AlphaMode::Blend ||
                (mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
            {
                TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
                transparencyDrawInfo.mesh = &mesh;
                transparencyDrawInfo.shaderId = drawInfo.shaderId;
                transparencyDrawInfo.color = drawInfo.color;

                // [BARU] Salin Data Matrix Override ke Transparency Info
                transparencyDrawInfo.useManualMatrix = drawInfo.useManualMatrix;
                transparencyDrawInfo.worldMatrix = drawInfo.worldMatrix;

                // カメラとの距離を算出 (Gunakan Matrix Manual jika ada untuk hitung jarak)
                DirectX::XMFLOAT4X4 transformMatrix = drawInfo.useManualMatrix ? drawInfo.worldMatrix : mesh.node->worldTransform;

                DirectX::XMVECTOR Position = DirectX::XMVectorSet(
                    transformMatrix._41,
                    transformMatrix._42,
                    transformMatrix._43,
                    0.0f);
                DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Position, CameraPosition);
                transparencyDrawInfo.distance = DirectX::XMVectorGetX(DirectX::XMVector3Dot(CameraFront, Vec));

                continue;
            }

            // 描画 (Pass data matrix ke lambda)
            drawMesh(mesh, shader, drawInfo.useManualMatrix, drawInfo.worldMatrix);
        }

        shader->End(rc);
    }
    drawInfos.clear();

    // ブレンドステート設定
    dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    // カメラから遠い順にソート
    std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
        [](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
        {
            return lhs.distance > rhs.distance;
        });

    // 半透明描画処理
    for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
    {
        Shader* shader = shaders[static_cast<int>(transparencyDrawInfo.shaderId)].get();
        shader->Begin(rc);

        CbObject cbObject;
        cbObject.color = transparencyDrawInfo.color;
        dc->UpdateSubresource(objectConstantBuffer.Get(), 0, 0, &cbObject, 0, 0);

        // 描画 (Pass data matrix ke lambda)
        drawMesh(*transparencyDrawInfo.mesh, shader, transparencyDrawInfo.useManualMatrix, transparencyDrawInfo.worldMatrix);

        shader->End(rc);
    }
    transparencyDrawInfos.clear();

    // 定数バッファ設定解除
    for (ID3D11Buffer*& vsConstantBuffer : vsConstantBuffers) { vsConstantBuffer = nullptr; }
    for (ID3D11Buffer*& psConstantBuffer : psConstantBuffers) { psConstantBuffer = nullptr; }
    dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
    dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

    // Unbind object buffer
    ID3D11Buffer* nullBuffer = nullptr;
    dc->PSSetConstantBuffers(2, 1, &nullBuffer);

    // サンプラステート設定解除
    for (ID3D11SamplerState*& samplerState : samplerStates) { samplerState = nullptr; }
    dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
}