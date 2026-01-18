#pragma once

#include <cstring>
#include <DirectXMath.h>
#include "System/Shader.h"

class UberShader : public Shader
{
public:
    // =========================================================
    // [USER SETTINGS] DEFAULTS
    // =========================================================
    static constexpr bool  DEFAULT_ENABLED      = false;
    static constexpr float DEFAULT_INTENSITY    = 0.38f;
    static constexpr float DEFAULT_SMOOTHNESS   = 0.2f;
    static constexpr bool  DEFAULT_ROUNDED      = false;
    static constexpr float DEFAULT_ROUNDNESS    = 1.0f;

    // Color default (r, g, b, a)
    static constexpr float DEFAULT_COLOR_R      = 0.0f;
    static constexpr float DEFAULT_COLOR_G      = 0.0f;
    static constexpr float DEFAULT_COLOR_B      = 0.0f;

    // =========================================================
    // CONSTANTS
    // =========================================================
    static constexpr float INTENSITY_FACTOR     = 3.0f;
    static constexpr float SMOOTHNESS_FACTOR    = 5.0f;
    static constexpr float SMOOTHNESS_MIN       = 0.000001f;
    static constexpr float ROUNDNESS_POWER      = 6.0f;
    static constexpr UINT  VERTEX_COUNT         = 3;

    // =========================================================
    // DATA STRUCTURE - "UberData" (Holds EVERYTHING)
    // =========================================================
    struct UberData
    {
        bool              enabled               = DEFAULT_ENABLED;
        DirectX::XMFLOAT4 color                 = { DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, 1.0f };
        DirectX::XMFLOAT2 center                = { 0.5f, 0.5f };
        float             intensity             = DEFAULT_INTENSITY;
        float             smoothness            = DEFAULT_SMOOTHNESS;
        bool              rounded               = DEFAULT_ROUNDED;
        float             roundness             = DEFAULT_ROUNDNESS;

        // Effects
        float             blurStrength          = 0.01f;
        float             distortion            = 0.015f;
        float             glitchStrength        = 0.01f;
        float             time                  = 0.0f;

        // Scanlines
        float             scanlineStrength      = 0.2f;
        float             scanlineSpeed         = 40.0f;
        float             scanlineSize          = 150.0f;
        float             fineOpacity           = 1.0f;
        float             fineDensity           = 30.0f;
        float             fineRotation          = 0.0f;
    };

    // =========================================================
    // PUBLIC METHODS
    // =========================================================
    UberShader(ID3D11Device* device);
    ~UberShader() override = default;

    void Begin(const RenderContext& rc) override {}
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override {}
    void End(const RenderContext& rc) override {}

    void Draw(ID3D11DeviceContext* dc, ID3D11ShaderResourceView* textureSRV, const UberData& data);

private:
    // =========================================================
    // INTERNAL BUFFER (Matches HLSL)
    // =========================================================
    struct CbUber
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 center;
        float intensity;
        float smoothness;
        float rounded;
        float roundness;
        float blurStrength;
        float distortion;
        float glitchStrength;
        float scanlineStrength;
        float time;
        float scanlineSpeed;
        float scanlineSize;
        float fineOpacity;
        float fineDensity;
        /*float padding;*/
        float fineRotation;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;

    UberData currentData = {};
};