#pragma once
#include "UIButton.h"
#include "Primitive.h"
#include "ResourceManager.h"
#include "BitmapFont.h"

enum class TextAlignment {
    Center,
    Left,
    Right
};

// 1. Struct untuk menampung Style per State
// Ini profesional karena mengelompokkan data yang berkaitan.
struct ButtonStyle {
    DirectX::XMFLOAT4 backColor;   // RGBA (Penting ada A untuk transparansi)
    DirectX::XMFLOAT4 borderColor; // RGBA
    DirectX::XMFLOAT4 textColor;   // RGBA
};

class UIButtonPrimitive : public UIButton
{
public:
    UIButtonPrimitive(Primitive* primRenderer, const std::string& text, float x, float y, float w, float h)
        : UIButton(x, y, w, h), primitive(primRenderer), labelText(text)
    {
        // Default Settings
        paddingX = 10.0f;
        alignment = TextAlignment::Center;
        verticalAdjustment = 3.0f;
        textScale = 0.625f;

        // Default Style (Fallback) - Bisa di-override nanti
        // Normal: Biru Tua, Hover: Biru Terang, Press: Hijau
        styles[0] = { {0.0f, 0.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }; // Standby
        styles[1] = { {0.0f, 0.0f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f} }; // Hover
        styles[2] = { {0.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f} }; // Pressed
    }

    // Setter Style yang Efisien (By Reference)
    void SetStyle(ButtonState state, const ButtonStyle& style)
    {
        // Convert enum class ke index array (0, 1, 2)
        int index = static_cast<int>(state);
        // Safety check (walaupun enum class sudah aman, defensif programming itu bagus)
        if (index >= 0 && index < 3) {
            styles[index] = style;
        }
    }

    // Helper cepat untuk set Text Color saja (jika perlu)
    void SetTextColor(ButtonState state, DirectX::XMFLOAT4 color) {
        int index = static_cast<int>(state);
        styles[index].textColor = color;
    }

    // --- Layout Setters ---
    void SetTextScale(float s) { textScale = s; }
    void SetAlignment(TextAlignment align) { alignment = align; }
    void SetPadding(float p) { paddingX = p; }
    void SetVerticalAdjustment(float y) { verticalAdjustment = y; }

    // --- Getters untuk ImGui/Debug ---
    const std::string& GetText() const { return labelText; }
    float& GetTextScale() { return textScale; }
    float& GetPadding() { return paddingX; }
    float& GetVerticalAdjustment() { return verticalAdjustment; }
    TextAlignment& GetAlignment() { return alignment; }
    ButtonStyle& GetStyle(ButtonState state) { return styles[static_cast<int>(state)]; }

    void Render(ID3D11DeviceContext* dc, Camera* camera) override
    {
        if (!primitive) return;

        // 2. Tentukan Style aktif berdasarkan State
        int stateIdx = static_cast<int>(currentState);
        const ButtonStyle& activeStyle = styles[stateIdx];

        // 3. Render Background & Border
        // Perhatikan parameter terakhir (Alpha) diambil dari activeStyle.backColor.w
        // Jika Normal Alpha-nya 0, maka background jadi invisible (transparan).
        if (activeStyle.backColor.w > 0.0f) // Optimasi: Jangan gambar jika full transparan
        {
            primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
                activeStyle.backColor.x, activeStyle.backColor.y, activeStyle.backColor.z, activeStyle.backColor.w);
        }

        // Render Border (Opsional: Bisa dikasih logika if alpha > 0 juga)
        primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
            activeStyle.borderColor.x, activeStyle.borderColor.y, activeStyle.borderColor.z, activeStyle.borderColor.w);

        primitive->Render(dc);

        // 4. Render Text
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            DirectX::XMFLOAT2 textSize = font->MeasureText(labelText, textScale);

            // Perhitungan Posisi X
            float textX = 0.0f;
            switch (alignment)
            {
            case TextAlignment::Left:   textX = (float)rect.left + paddingX; break;
            case TextAlignment::Right:  textX = (float)rect.left + width - textSize.x - paddingX; break;
            case TextAlignment::Center: default: textX = (float)rect.left + (width - textSize.x) / 2.0f; break;
            }

            // Perhitungan Posisi Y + Efek Tekan
            float pressOffset = (currentState == ButtonState::PRESSED) ? 2.0f : 0.0f;
            float textY = (float)rect.top + (height - textSize.y) / 2.0f + verticalAdjustment + pressOffset;

            // Render Text dengan Warna dari Style
            font->Draw(labelText.c_str(), textX, textY, textScale,
                activeStyle.textColor.x, activeStyle.textColor.y, activeStyle.textColor.z, activeStyle.textColor.w);
        }
    }

private:
    Primitive* primitive;
    std::string labelText;

    // Array style untuk 3 state: STANDBY, HOVER, PRESSED
    // Menggunakan array lebih cepat aksesnya daripada if-else branch
    ButtonStyle styles[3];

    float textScale;
    float paddingX;
    TextAlignment alignment;
    float verticalAdjustment;
};