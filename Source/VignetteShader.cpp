#include "VignetteShader.h"
#include "System/GpuResourceUtils.h" 
#include "System/Misc.h"
#include <algorithm> 

VignetteShader::VignetteShader(ID3D11Device* device)
{
    // --------------------------------------------------------
    // Load Shaders
    // --------------------------------------------------------
    GpuResourceUtils::LoadVertexShader(device, "Data/Shader/VignetteVS.cso", nullptr, 0, nullptr, vertexShader.GetAddressOf());
    GpuResourceUtils::LoadPixelShader(device, "Data/Shader/VignettePS.cso", pixelShader.GetAddressOf());

    // --------------------------------------------------------
    // Create Constant Buffer
    // --------------------------------------------------------
    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbVignette), constantBuffer.GetAddressOf());

    // --------------------------------------------------------
    // Create Sampler State
    // --------------------------------------------------------
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&samplerDesc, samplerState.GetAddressOf());

    // Force an update on the first frame
    currentData.intensity = -1.0f;
}

void VignetteShader::Draw(ID3D11DeviceContext* dc, ID3D11ShaderResourceView* textureSRV, const VignetteData& data)
{
    // --------------------------------------------------------
    // OPTIMIZATION: Update GPU Buffer only if data changed
    // --------------------------------------------------------
    if (memcmp(&data, &currentData, sizeof(VignetteData)) != 0)
    {
        CbVignette cb = {};
        cb.color = data.color;
        cb.center = data.center;

        // Apply Tuning Multipliers
        cb.intensity = data.intensity * INTENSITY_FACTOR;
        cb.smoothness = (std::max)(SMOOTHNESS_MIN, data.smoothness * SMOOTHNESS_FACTOR);

        // Convert Boolean to Float
        cb.rounded = data.rounded ? 1.0f : 0.0f;

        // Calculate Roundness Curve (Lerp Logic)
        cb.roundness = ROUNDNESS_POWER * (1.0f - data.roundness) + data.roundness;

        cb.blurStrength = std::clamp(data.blurStrength, 0.0f, 0.1f);

        dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cb, 0, 0);

        // Update Cache
        currentData = data;
    }

    // --------------------------------------------------------
    // SETUP PIPELINE
    // --------------------------------------------------------
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // OPTIMIZATION: Unbind Input Layout & Vertex Buffers
    // The Vertex Shader generates coordinates using SV_VertexID, so no input data is needed.
    dc->IASetInputLayout(nullptr);
    ID3D11Buffer* nullBuffer = nullptr;
    UINT stride = 0, offset = 0;
    dc->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);

    // --------------------------------------------------------
    // BIND RESOURCES
    // --------------------------------------------------------
    dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc->PSSetShaderResources(0, 1, &textureSRV);
    dc->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    // --------------------------------------------------------
    // DRAW FULL SCREEN TRIANGLE
    // --------------------------------------------------------
    dc->Draw(VERTEX_COUNT, 0);
}