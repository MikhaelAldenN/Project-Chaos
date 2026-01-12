#pragma once

#include <cstring>
#include <DirectXMath.h>
#include "System/Shader.h"

class VignetteShader : public Shader
{
public:
    // =========================================================
    // [USER SETTINGS] CHANGE DEFAULT VALUES HERE
    // =========================================================
    static constexpr bool  DEFAULT_ENABLED      = false;
    static constexpr float DEFAULT_INTENSITY    = 0.38f;
    static constexpr float DEFAULT_SMOOTHNESS   = 0.2f;
    static constexpr bool  DEFAULT_ROUNDED      = true;
    static constexpr float DEFAULT_ROUNDNESS    = 1.0f;

    // Color default (r, g, b, a)
    static constexpr float DEFAULT_COLOR_R      = 0.0f;
    static constexpr float DEFAULT_COLOR_G      = 0.0f;
    static constexpr float DEFAULT_COLOR_B      = 0.0f;

    // =========================================================
    // CONSTANTS & TUNING PARAMETERS
    // =========================================================
    // These multipliers adjust the 0.0-1.0 slider inputs to useful shader ranges
    static constexpr float INTENSITY_FACTOR     = 3.0f;         
    static constexpr float SMOOTHNESS_FACTOR    = 5.0f;        
    static constexpr float SMOOTHNESS_MIN       = 0.000001f;      
    static constexpr float ROUNDNESS_POWER      = 6.0f;          
    static constexpr UINT  VERTEX_COUNT         = 3;                

    // =========================================================
    // DATA STRUCTURES
    // =========================================================
    struct VignetteData
    {
        bool              enabled       = DEFAULT_ENABLED;
        DirectX::XMFLOAT4 color         = { DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, 1.0f };
        DirectX::XMFLOAT2 center        = { 0.5f, 0.5f };
        float             intensity     = DEFAULT_INTENSITY;
        float             smoothness    = DEFAULT_SMOOTHNESS;
        bool              rounded       = DEFAULT_ROUNDED;
        float             roundness     = DEFAULT_ROUNDNESS;
    };

    // =========================================================
    // PUBLIC METHODS
    // =========================================================
    VignetteShader(ID3D11Device* device);
    ~VignetteShader() override = default;

    // Standard Shader Interface (Unused for Post-Process)
    void Begin(const RenderContext& rc) override {}
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override {}
    void End(const RenderContext& rc) override {}

    // Special Draw function for Post-Processing
    void Draw(ID3D11DeviceContext* dc, ID3D11ShaderResourceView* textureSRV, const VignetteData& data);

private:
    // =========================================================
    // INTERNAL STRUCTURES & RESOURCES
    // =========================================================
    struct CbVignette
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 center;
        float intensity;
        float smoothness;
        float rounded;
        float roundness;
        DirectX::XMFLOAT2 padding;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;

    // Cache for Optimization
    VignetteData currentData = {};
};