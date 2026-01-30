#include "UIImpactDisplay.h"
#include <algorithm>
#include <cmath>

UIImpactDisplay::UIImpactDisplay() {}
UIImpactDisplay::~UIImpactDisplay() {}

void UIImpactDisplay::Initialize(ID3D11Device* device)
{
    // Init Primitive untuk Flash
    m_primitive = std::make_unique<Primitive>(device);

    auto LoadSprite = [&](ImpactType type, const char* path) {
        auto sprite = std::make_unique<Sprite>(device, path);
        // Sesuaikan ukuran sprite manual atau getter
        float w = 1920.0f;
        float h = 1080.0f;
        m_sprites[type] = std::move(sprite);
        m_spriteDims[type] = { w, h };
        };

    LoadSprite(ImpactType::Super, "Data/Sprite/Scene Breaker/Sprite_Text_ESCAPE.png");
    // Load sprite lainnya...

    ResetAnim();
}

void UIImpactDisplay::Show(ImpactType type, float duration)
{
    if (m_sprites.find(type) == m_sprites.end()) return;

    m_anim.isActive = true;
    m_anim.timer = 0.0f;
    m_anim.totalDuration = duration;
    m_anim.currentType = type;

    // Setup awal
    m_anim.currentScale = 5.0f;
    m_anim.currentAlpha = 1.0f;
    m_anim.currentRotation = 0.0f; // Stabil, tidak miring
    m_anim.flashAlpha = 0.8f;      // Mulai dengan flash terang (putih 80%)
}

void UIImpactDisplay::Update(float elapsedTime)
{
    if (!m_anim.isActive) return;

    m_anim.timer += elapsedTime;

    // --- 1. Update Flash Logic ---
    // Flash menghilang sangat cepat (0.1 detik)
    if (m_anim.flashAlpha > 0.0f)
    {
        m_anim.flashAlpha -= elapsedTime * 8.0f; // Kecepatan fade out flash
        if (m_anim.flashAlpha < 0.0f) m_anim.flashAlpha = 0.0f;
    }

    // --- 2. Update Text Animation ---
    float impactTime = 0.15f;

    if (m_anim.timer < impactTime)
    {
        // FASE 1: SLAM (Membesar ke Normal)
        float t = m_anim.timer / impactTime;
        float k = 15.0f; // Lebih cepat/snappy
        float scaleFactor = exp(-k * t);

        // Scale turun drastis dari 5.0 ke 1.0
        m_anim.currentScale = 1.0f + (4.0f * scaleFactor);
    }
    else if (m_anim.timer < m_anim.totalDuration)
    {
        // FASE 2: SLOW ZOOM OUT (Pelan banget)
        // Setelah impact, scale 1.0 pelan-pelan jadi 0.9 (menjauh)
        float timeAfterImpact = m_anim.timer - impactTime;
        float zoomSpeed = 0.05f; // Semakin kecil semakin pelan

        m_anim.currentScale = 1.0f - (timeAfterImpact * zoomSpeed);

        // Fade out di akhir durasi
        float timeLeft = m_anim.totalDuration - m_anim.timer;
        if (timeLeft < 0.2f)
        {
            m_anim.currentAlpha = timeLeft / 0.2f;
        }
    }
    else
    {
        ResetAnim();
    }
}

void UIImpactDisplay::Render(ID3D11DeviceContext* dc)
{
    if (!m_anim.isActive) return;

    // =========================================================
    // LAYER 1: BACKGROUND (Hitam Transparan)
    // =========================================================
    // Tujuannya menggelapkan layar game di belakang agar teks terbaca jelas.

    // Kita buat background ikut fade out di akhir animasi (saat text fade out)
    float bgAlpha = 0.3f; // Opacity background (75% gelap)

    // Jika animasi mau selesai (fade out phase), kurangi alpha background juga
    if (m_anim.currentAlpha < 1.0f) {
        bgAlpha *= m_anim.currentAlpha;
    }

    // Gambar Kotak Fullscreen (0,0,0 = Hitam)
    m_primitive->Rect(
        0.0f, 0.0f,           // Posisi (Kiri Atas)
        m_screenW, m_screenH, // Ukuran (Fullscreen)
        0.0f, 0.0f,           // Center Pivot (Default 0,0)
        0.0f,                 // Angle
        0.0f, 0.0f, 0.0f,     // Warna RGB (HITAM)
        bgAlpha               // Alpha
    );

    // Push ke GPU
    m_primitive->Render(dc);

    // =========================================================
    // LAYER 2: FLASH (Putih Transparan)
    // =========================================================
    // Efek kilatan cahaya di awal impact
    if (m_anim.flashAlpha > 0.0f)
    {
        m_primitive->Rect(
            0.0f, 0.0f,
            m_screenW, m_screenH,
            0.0f, 0.0f,
            0.0f,
            1.0f, 1.0f, 1.0f, // Warna RGB (PUTIH)
            m_anim.flashAlpha // Alpha Flash
        );
        m_primitive->Render(dc);
    }

    // =========================================================
    // LAYER 3: TEXT (Sprite)
    // =========================================================
    // Teks muncul paling depan (di atas hitam dan flash)
    auto it = m_sprites.find(m_anim.currentType);
    if (it != m_sprites.end())
    {
        Sprite* sprite = it->second.get();
        SpriteDim dim = m_spriteDims[m_anim.currentType];

        float finalW = dim.w * m_anim.currentScale;
        float finalH = dim.h * m_anim.currentScale;

        // Hitung Center Screen
        float centerX = m_screenW * 0.5f;
        float centerY = m_screenH * 0.5f;

        float dx = centerX - (finalW * 0.5f);
        float dy = centerY - (finalH * 0.5f);

        sprite->Render(dc,
            dx, dy, 0.0f,
            finalW, finalH,
            m_anim.currentRotation,
            1.0f, 1.0f, 1.0f, m_anim.currentAlpha
        );
    }
}

void UIImpactDisplay::OnResize(float screenW, float screenH)
{
    m_screenW = screenW;
    m_screenH = screenH;
}

bool UIImpactDisplay::IsActive() const
{
    return m_anim.isActive;
}

void UIImpactDisplay::ResetAnim()
{
    m_anim.isActive = false;
    m_anim.flashAlpha = 0.0f;
    m_anim.currentScale = 1.0f;
    m_anim.currentAlpha = 1.0f;
}