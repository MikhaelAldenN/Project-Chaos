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
    // STEP 2: LENS DISTORTION & RGB SPLIT
    // -----------------------------------------------------
    // We use the separate 'distortion' variable now
    float distStrength = fx_distortion;

    float2 uvR = LensDistortion(uv, distStrength * 1.0f);
    float2 uvG = LensDistortion(uv, distStrength * 1.1f); 
    float2 uvB = LensDistortion(uv, distStrength * 1.2f);

    // Black Border Check 
    if (uvG.x < 0.0 || uvG.x > 1.0 || uvG.y < 0.0 || uvG.y > 1.0)
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
        // No Blur: Just sample the RGB split
        finalColor.r = sceneTexture.Sample(samplerState, uvR).r;
        finalColor.g = sceneTexture.Sample(samplerState, uvG).g;
        finalColor.b = sceneTexture.Sample(samplerState, uvB).b;
    }

    // -----------------------------------------------------
    // STEP 5A: ROLLING BAR (Animation)
    // -----------------------------------------------------
    if (crt_scanlineOpacity > 0.0f)
    {
        // Use uvG.y for curvature
        float bar = sin(uvG.y * 3.0f - crt_time * crt_scanlineSpeed);
        bar = (bar + 1.0f) * 0.5f;
        
        // Apply Sharpness (Size)
        bar = pow(bar, max(1.0f, crt_scanlineSize));
        
        // Subtract from color
        finalColor.rgb -= bar * crt_scanlineOpacity * 0.5f;
    }

// -----------------------------------------------------
    // STEP 5B: FINE SCANLINES (Static Mesh)
    // -----------------------------------------------------
    if (crt_fineOpacity > 0.0f)
    {
        // [BARU] ROTATION LOGIC
        // 1. Buat Matriks Rotasi (2D)
        float s = sin(fineRotation);
        float c = cos(fineRotation);
        float2x2 rotationMatrix = float2x2(c, -s, s, c);

        // 2. Pusatkan UV ke tengah (0.5, 0.5) agar berputar pada poros tengah layar
        float2 centeredUV = uvG - 0.5f;

        // 3. Terapkan Rotasi
        float2 rotatedUV = mul(centeredUV, rotationMatrix);

        // 4. Kembalikan posisi UV
        rotatedUV += 0.5f;

        // [UPDATE] Gunakan 'rotatedUV.y' menggantikan 'uvG.y'
        // High frequency sine wave
        float mesh = sin(rotatedUV.y * crt_fineDensity * 50.0f);
        mesh = (mesh + 1.0f) * 0.5f;
        
        // Slight sharpness
        mesh = pow(mesh, 1.2f);

        // Apply dark mesh lines
        finalColor.rgb -= (1.0f - mesh) * crt_fineOpacity * 0.3f;
    }
    
    // -----------------------------------------------------
    // STEP 6: APPLY VIGNETTE COLOR
    // -----------------------------------------------------
    // Apply vignette last so corners stay dark
    finalColor.rgb = lerp(v_color.rgb, finalColor.rgb, mask);
    
    return finalColor;
}