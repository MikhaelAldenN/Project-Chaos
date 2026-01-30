#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include "System/Sprite.h"
#include "Primitive.h" // Include Primitive

enum class ImpactType {
    None,
    Super,
    Hot,
    MashSpace,
    Ready,
    Go
};

class UIImpactDisplay
{
public:
    UIImpactDisplay();
    ~UIImpactDisplay();

    void Initialize(ID3D11Device* device);
    void Show(ImpactType type, float duration = 1.5f); // Hapus param intensity (shake)

    void Update(float elapsedTime);
    void Render(ID3D11DeviceContext* dc);
    void OnResize(float screenW, float screenH);
    bool IsActive() const;

private:
    struct AnimationState {
        bool isActive = false;
        float timer = 0.0f;
        float totalDuration = 0.0f;
        ImpactType currentType = ImpactType::None;

        // Variabel animasi
        float currentScale = 1.0f;
        float currentAlpha = 1.0f;
        float currentRotation = 0.0f;
        float flashAlpha = 0.0f; // Untuk efek flash putih
    };

    std::unordered_map<ImpactType, std::unique_ptr<Sprite>> m_sprites;
    struct SpriteDim { float w, h; };
    std::unordered_map<ImpactType, SpriteDim> m_spriteDims;

    // Tambahan Primitive untuk Flash
    std::unique_ptr<Primitive> m_primitive;

    AnimationState m_anim;

    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    void ResetAnim();
};