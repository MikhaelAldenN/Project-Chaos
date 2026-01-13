cbuffer VignetteConstantBuffer : register(b0)
{
    float4 vignette_color;
    float2 vignette_center;
    float vignette_intensity;
    float vignette_smoothness;
    float vignette_rounded;
    float vignette_roundness;
    float vignette_blur_strength;
    float vignette_distortion;
    
    // CRT Settings packed here
    float gl_strength;      
    float gl_scanline;      
    float gl_time;          
    float gl_speed;         
    float gl_size;          
    
    // Fine Scanline Settings
    float gl_fine_opacity; 
    float gl_fine_density; 
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

// Random Noise Generator
float rand(float2 n)
{
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

// CRT / Barrel Distortion
float2 LensDistortion(float2 uv, float k)
{
    float2 t = uv - vignette_center;
    float r2 = t.x * t.x + t.y * t.y;
    float f = 1.0 + r2 * (k * 10.0f);
    return vignette_center + t * f;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    // =========================================================
    // STEP 1: GLITCH JITTER (Shake the image horizontally)
    // =========================================================
    if (gl_strength > 0.0f)
    {
        // Random shake based on Time and Y position (Tearing effect)
        float shake = (rand(float2(0, uv.y + gl_time)) - 0.5f) * gl_strength * 0.1f;
        uv.x += shake;
    }

    // =========================================================
    // STEP 2: DISTORTION & RGB SPLIT
    // =========================================================
    float distortionStrength = vignette_distortion;

    float2 uvR = LensDistortion(uv, distortionStrength * 1.0f);
    float2 uvG = LensDistortion(uv, distortionStrength * 1.1f);
    float2 uvB = LensDistortion(uv, distortionStrength * 1.2f);

    // Black Border Check
    if (uvG.x < 0.0 || uvG.x > 1.0 || uvG.y < 0.0 || uvG.y > 1.0)
    {
        return float4(0, 0, 0, 1);
    }

    // =========================================================
    // STEP 3: VIGNETTE MASK CALCULATION
    // =========================================================
    float width, height;
    sceneTexture.GetDimensions(width, height);
    float2 correctedCoord = (uvG - vignette_center);
    correctedCoord.x *= lerp(1.0f, width / height, vignette_rounded);
    float distSq = dot(correctedCoord, correctedCoord);
    float mask = saturate(1.0f - distSq * vignette_intensity);
    mask = smoothstep(0.0f, vignette_smoothness, mask);

    // =========================================================
    // STEP 4: SAMPLE TEXTURE (Blur + Color)
    // =========================================================
    float4 finalColor = float4(0, 0, 0, 1);

    if (vignette_blur_strength > 0.0f)
    {
        float blurAmount = vignette_blur_strength * distSq * 4.0f;
        float3 accumColor = float3(0, 0, 0);

        [unroll]
        for (int i = 0; i < BLUR_SAMPLES; i++)
        {
            float scale = 1.0f - blurAmount * (float(i) / float(BLUR_SAMPLES - 1));
            
            accumColor.r += sceneTexture.Sample(samplerState, vignette_center + (uvR - vignette_center) * scale).r;
            accumColor.g += sceneTexture.Sample(samplerState, vignette_center + (uvG - vignette_center) * scale).g;
            accumColor.b += sceneTexture.Sample(samplerState, vignette_center + (uvB - vignette_center) * scale).b;
        }
        finalColor.rgb = accumColor / float(BLUR_SAMPLES);
    }
    else
    {
        finalColor.r = sceneTexture.Sample(samplerState, uvR).r;
        finalColor.g = sceneTexture.Sample(samplerState, uvG).g;
        finalColor.b = sceneTexture.Sample(samplerState, uvB).b;
    }

    // =========================================================
    // STEP 5A: ROLLING BAR (The Hum Bar Animation)
    // =========================================================
    if (gl_scanline > 0.0f)
    {
        // Use uvG for curvature
        float bar = sin(uvG.y * 3.0f - gl_time * gl_speed);
        bar = (bar + 1.0f) * 0.5f;
        // Sharpness based on Size
        bar = pow(bar, max(1.0f, gl_size));
        // Subtract color
        finalColor.rgb -= bar * gl_scanline * 0.5f;
    }

    // =========================================================
    // STEP 5B: FINE SCANLINES (The Static CRT Mesh)
    // =========================================================
    if (gl_fine_opacity > 0.0f)
    {
        // High frequency sine wave (Density * 50)
        float mesh = sin(uvG.y * gl_fine_density * 50.0f);
        mesh = (mesh + 1.0f) * 0.5f;
        
        // Slight sharpness
        mesh = pow(mesh, 1.2f);

        // Apply dark lines
        finalColor.rgb -= (1.0f - mesh) * gl_fine_opacity * 0.3f;
    }

    // =========================================================
    // STEP 6: APPLY VIGNETTE (Final Composition)
    // =========================================================
    // We apply vignette last so the corners stay dark and don't get scanlines
    finalColor.rgb = lerp(vignette_color.rgb, finalColor.rgb, mask);
    
    return finalColor;
}