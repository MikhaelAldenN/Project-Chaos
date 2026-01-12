cbuffer VignetteConstantBuffer : register(b0)
{
    float4 vignette_color;
    float2 vignette_center;
    float vignette_intensity;
    float vignette_smoothness;
    float vignette_rounded;
    float vignette_roundness;
    float vignette_blur_strength;
    float padding;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D sceneTexture : register(t0);
SamplerState samplerState : register(s0);

static const int BLUR_SAMPLES = 10;

// =========================================================
// CRT / BARREL DISTORTION FUNCTION
// =========================================================
float2 LensDistortion(float2 uv, float k)
{
    float2 t = uv - vignette_center;
    float r2 = t.x * t.x + t.y * t.y; // Distance squared
    
    // [FIX]: Use Multiplication (1 + k*r2)
    // This creates Barrel Distortion (Bulging OUT like a CRT TV).
    // The further from center (r2), the more we pull from the outside (f > 1).
    // We multiply k by a constant (e.g. 5.0) to make the effect visible.
    float f = 1.0 + r2 * (k * 10.0f);
    
    return vignette_center + t * f;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    // =========================================================
    // STEP 1: CALCULATE DISTORTION (CHROMATIC ABERRATION)
    // =========================================================
    // Use blur strength as the distortion driver
    float distortionStrength = vignette_blur_strength;

    // Calculate 3 separate coordinates for Red, Green, Blue
    // This splits the colors at the edges (TV Effect)
    float2 uvR = LensDistortion(pin.texcoord, distortionStrength * 1.0f);
    float2 uvG = LensDistortion(pin.texcoord, distortionStrength * 1.1f);
    float2 uvB = LensDistortion(pin.texcoord, distortionStrength * 1.2f); // Blue distorts most

    // Use Green channel as the "Main" coordinate for masking checks
    float2 mainUV = uvG;

    // 1. Black Border (TV Bezel)
    // If we try to sample outside the screen (0-1), return Black.
    if (mainUV.x < 0.0 || mainUV.x > 1.0 || mainUV.y < 0.0 || mainUV.y > 1.0)
    {
        return float4(0, 0, 0, 1);
    }

    // =========================================================
    // STEP 2: VIGNETTE MASK (Using Distorted UV)
    // =========================================================
    float width, height;
    sceneTexture.GetDimensions(width, height);
    
    float2 coord = mainUV - vignette_center;
    float2 correctedCoord = coord;
    
    // Aspect Ratio Fix
    correctedCoord.x *= lerp(1.0f, width / height, vignette_rounded);
    
    float distSq = dot(correctedCoord, correctedCoord);
    float mask = saturate(1.0f - distSq * vignette_intensity);
    mask = smoothstep(0.0f, vignette_smoothness, mask);

    // =========================================================
    // STEP 3: RADIAL BLUR + RGB SPLIT SAMPLE
    // =========================================================
    float4 finalColor = float4(0, 0, 0, 1);

    if (vignette_blur_strength > 0.0f)
    {
        // Radial Blur Amount
        float blurAmount = vignette_blur_strength * distSq * 4.0f;
        
        float3 accumColor = float3(0, 0, 0);

        [unroll]
        for (int i = 0; i < BLUR_SAMPLES; i++)
        {
            float scale = 1.0f - blurAmount * (float(i) / float(BLUR_SAMPLES - 1));
            
            // Sample R, G, B using their own distorted UVs
            float r = sceneTexture.Sample(samplerState, vignette_center + (uvR - vignette_center) * scale).r;
            float g = sceneTexture.Sample(samplerState, vignette_center + (uvG - vignette_center) * scale).g;
            float b = sceneTexture.Sample(samplerState, vignette_center + (uvB - vignette_center) * scale).b;
            
            accumColor += float3(r, g, b);
        }
        finalColor.rgb = accumColor / float(BLUR_SAMPLES);
    }
    else
    {
        // No Blur, Just RGB Split Distortion
        float r = sceneTexture.Sample(samplerState, uvR).r;
        float g = sceneTexture.Sample(samplerState, uvG).g;
        float b = sceneTexture.Sample(samplerState, uvB).b;
        finalColor.rgb = float3(r, g, b);
    }

    // =========================================================
    // STEP 4: APPLY VIGNETTE DARKNESS
    // =========================================================
    finalColor.rgb = lerp(vignette_color.rgb, finalColor.rgb, mask);
    
    return finalColor;
}