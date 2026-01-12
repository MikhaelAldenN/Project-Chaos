cbuffer VignetteConstantBuffer : register(b0)
{
    float4 vignette_color;
    float2 vignette_center;
    float vignette_intensity;
    float vignette_smoothness;
    float vignette_rounded;
    float vignette_roundness;
    float2 padding;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D sceneTexture : register(t0);
SamplerState samplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = sceneTexture.Sample(samplerState, pin.texcoord);
    
    // Calculate vector from center
    float2 coord = pin.texcoord - vignette_center;
    
    // Aspect ratio correction (Pre-calculate width/height CPU side if possible)
    float width, height;
    sceneTexture.GetDimensions(width, height);
    coord.x *= lerp(1.0f, width / height, vignette_rounded);
    
    // Distance squared
    float distSq = dot(coord, coord);
    
    // Cheap Falloff approximation
    float mask = saturate(1.0f - distSq * vignette_intensity);
    
    // Smoothstep 
    mask = smoothstep(0.0f, vignette_smoothness, mask);
    
    // Apply lerp
    color.rgb = lerp(vignette_color.rgb, color.rgb, mask);
    
    return color;
}