cbuffer UberConstantBuffer : register(b0)
{
    // --- VIGNETTE & COLOR ---
    float4 v_color;
    float2 v_center;
    float v_intensity;
    float v_smoothness;
    float v_rounded; // 1.0 = true, 0.0 = false
    float v_roundness;
    
    // --- LENS & GLITCH FX ---
    float fx_blurStrength;
    float fx_chromaticAberration;
    float fx_distortion;
    float fx_glitchStrength;
    
    // --- CRT SETTINGS ---
    float crt_scanlineOpacity; 
    float crt_time;
    float crt_scanlineSpeed;
    float crt_scanlineSize; 
    
    float crt_fineOpacity; 
    float crt_fineDensity; 
    
    float fineRotation; 
    
    // --- HDR BLOOM ---
    float bloomThreshold;
    float bloomIntensity;
    float padding_bloom;
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
// HELPER FUNCTIONS
// =========================================================

// Random Noise Generator
float rand(float2 n)
{
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

// CRT / Fish Eye Distortion
float2 LensDistortion(float2 uv, float k)
{
    float2 t = uv - v_center;
    float r2 = t.x * t.x + t.y * t.y;
    float f = 1.0 + r2 * (k * 10.0f);
    return v_center + t * f;
}

// =========================================================
// AAA GAUSSIAN BLOOM EXTRACTOR
// =========================================================
float3 SampleBloom(float2 uv, float2 texelSize, float radius)
{
    // A highly optimized 9-tap Gaussian approximation filter
    float2 offsets[9] =
    {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1, 0), float2(0, 0), float2(1, 0),
        float2(-1, 1), float2(0, 1), float2(1, 1)
    };
    float weights[9] =
    {
        0.0625, 0.125, 0.0625,
        0.125, 0.25, 0.125,
        0.0625, 0.125, 0.0625
    };
    
    float3 bloom = 0;
    
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float3 c = sceneTexture.SampleLevel(samplerState, uv + offsets[i] * texelSize * radius, 0).rgb;
        // Measure real brightness
        float brightness = dot(c, float3(0.2126, 0.7152, 0.0722));
        // Extract only the pixels that are violently bright (brighter than the threshold)
        float contribution = max(0.0f, brightness - bloomThreshold);
        
        bloom += (c * (contribution / max(brightness, 0.0001f))) * weights[i];
    }
    return bloom;
}

// =========================================================
// MAIN SHADER
// =========================================================
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    // -----------------------------------------------------
    // STEP 1: GLITCH JITTER (Shake)
    // -----------------------------------------------------
    if (fx_glitchStrength > 0.0f)
    {
        // Random horizontal shake based on Time + Y position
        float shake = (rand(float2(0, uv.y + crt_time)) - 0.5f) * fx_glitchStrength * 0.1f;
        uv.x += shake;
    }

    // -----------------------------------------------------
    // STEP 2: LENS DISTORTION & CHROMATIC ABERRATION
    // -----------------------------------------------------
    // Calculate Base UV with Lens Distortion (if active)
    float2 distortedUV = LensDistortion(uv, fx_distortion);

    // --- [UNIFORM / LATERAL CHROMATIC ABERRATION]---
    
    // A small multiplier to make the slider values easier to manage in the GUI
    float shiftAmount = fx_chromaticAberration * 0.5f;

    // Red shifts Left, Blue shifts Right (relative to center Green)
    float2 uvR = distortedUV + float2(shiftAmount, 0.0f);
    float2 uvG = distortedUV; 
    float2 uvB = distortedUV - float2(shiftAmount, 0.0f);

    // Black Border Check 
    [flatten]
    if (any(uvR < 0.0) || any(uvR > 1.0) || any(uvB < 0.0) || any(uvB > 1.0))
    {
        return float4(0, 0, 0, 1);
    }

    // -----------------------------------------------------
    // STEP 3: VIGNETTE MASK CALCULATION
    // -----------------------------------------------------
    float width, height;
    sceneTexture.GetDimensions(width, height);
    
    float2 coord = uvG - v_center;
    float2 correctedCoord = coord;
    
    // Apply Aspect Ratio Correction based on Roundness setting
    correctedCoord.x *= lerp(1.0f, width / height, v_rounded);
    
    float distSq = dot(correctedCoord, correctedCoord);
    float mask = saturate(1.0f - distSq * v_intensity);
    mask = smoothstep(0.0f, v_smoothness, mask);

    // -----------------------------------------------------
    // STEP 4: SAMPLE TEXTURE (Blur + Color)
    // -----------------------------------------------------
    float4 finalColor = float4(0, 0, 0, 1);

    if (fx_blurStrength > 0.0f)
    {
        // Radial Blur Logic
        float blurAmount = fx_blurStrength * distSq * 4.0f;
        float3 accumColor = float3(0, 0, 0);

        [unroll]
        for (int i = 0; i < BLUR_SAMPLES; i++)
        {
            float scale = 1.0f - blurAmount * (float(i) / float(BLUR_SAMPLES - 1));
            
            // Sample R, G, B separately using distorted UVs
            accumColor.r += sceneTexture.Sample(samplerState, v_center + (uvR - v_center) * scale).r;
            accumColor.g += sceneTexture.Sample(samplerState, v_center + (uvG - v_center) * scale).g;
            accumColor.b += sceneTexture.Sample(samplerState, v_center + (uvB - v_center) * scale).b;
        }
        finalColor.rgb = accumColor / float(BLUR_SAMPLES);
    }
    else
    {
        finalColor.r = sceneTexture.Sample(samplerState, uvR).r;
        finalColor.g = sceneTexture.Sample(samplerState, uvG).g;
        finalColor.b = sceneTexture.Sample(samplerState, uvB).b;
    }

    // -----------------------------------------------------
    // STEP 5A: ROLLING BAR (Animation)
    // -----------------------------------------------------
    if (crt_scanlineOpacity > 0.0f)
    {
        float bar = sin(uvG.y * 3.0f - crt_time * crt_scanlineSpeed);
        bar = (bar + 1.0f) * 0.5f;
        bar = pow(bar, max(1.0f, crt_scanlineSize));
        
        // [FIX] MULTIPLY instead of Subtract! This preserves HDR color ratios.
        finalColor.rgb *= (1.0f - bar * crt_scanlineOpacity * 0.5f);
    }

    // -----------------------------------------------------
    // STEP 5B: FINE SCANLINES (Static Mesh)
    // -----------------------------------------------------
    if (crt_fineOpacity > 0.0f)
    {
        float s = sin(fineRotation);
        float c = cos(fineRotation);
        float2x2 rotationMatrix = float2x2(c, -s, s, c);
        float2 centeredUV = uvG - 0.5f;
        float2 rotatedUV = mul(centeredUV, rotationMatrix);
        rotatedUV += 0.5f;

        float mesh = sin(rotatedUV.y * crt_fineDensity * 50.0f);
        mesh = (mesh + 1.0f) * 0.5f;
        mesh = pow(mesh, 1.2f);

        // [FIX] MULTIPLY instead of Subtract!
        finalColor.rgb *= (1.0f - (1.0f - mesh) * crt_fineOpacity * 0.3f);
    }
    
    // =========================================================
    // STEP 6: APPLY GAUSSIAN BLOOM
    // =========================================================
    float2 texelSize = 1.0f / float2(width, height);
    float3 bloomColor = 0;
    
    bloomColor += SampleBloom(uvG, texelSize, 2.0f);
    bloomColor += SampleBloom(uvG, texelSize, 6.0f);
    bloomColor += SampleBloom(uvG, texelSize, 12.0f);

    finalColor.rgb += (bloomColor * bloomIntensity);

    // Apply Vignette Mask over the bloom
    finalColor.rgb = lerp(v_color.rgb, finalColor.rgb, mask);
    
    // =========================================================
    // STEP 7: ACES FILMIC TONEMAPPING (HDR -> LDR)
    // =========================================================
    // [FIX] CAMERA EXPOSURE! 
    // This artificially boosts your dark scene before it hits the filmic curve.
    // If the scene is still too dark, change this to 2.5f or 3.0f!
    float exposure = 2.0f;
    finalColor.rgb *= exposure;
    
    finalColor.rgb = saturate((finalColor.rgb * (2.51f * finalColor.rgb + 0.03f)) /
                              (finalColor.rgb * (2.43f * finalColor.rgb + 0.59f) + 0.14f));
    
    return finalColor;
}