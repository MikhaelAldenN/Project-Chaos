#pragma once
#include "UIButton.h"
#include "Primitive.h"
#include "ResourceManager.h" // Asumsi untuk Font
#include "BitmapFont.h"

enum class TextAlignment {
    Center,
    Left,
    Right
};

class UIButtonPrimitive : public UIButton
{
public:
    // Constructor menerima pointer ke Primitive renderer
    UIButtonPrimitive(Primitive* primRenderer, const std::string& text, float x, float y, float w, float h)
        : UIButton(x, y, w, h), primitive(primRenderer), labelText(text)
    {
        // Default Colors (Style DOS/Terminal)
        backColor = { 0.0f, 0.0f, 0.5f }; // Biru Tua
        hoverColor = { 0.0f, 0.0f, 0.8f }; // Biru Terang
        pressColor = { 0.0f, 0.5f, 0.0f }; // Hijau
        borderColor = { 1.0f, 1.0f, 1.0f }; // Putih
    }

    // Setter untuk kustomisasi warna manual
    void SetColors(const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& hover, const DirectX::XMFLOAT3& pressed)
    {
        backColor = normal;
        hoverColor = hover;
        pressColor = pressed;
    }

    void SetTextScale(float s) { textScale = s; }
    float& GetTextScale() { return textScale; }

    void SetAlignment(TextAlignment align) { alignment = align; }
    TextAlignment& GetAlignment() { return alignment; } // Return reference untuk ImGui

    void SetPadding(float p) { paddingX = p; }
    float& GetPadding() { return paddingX; }

    void SetVerticalAdjustment(float y) { verticalAdjustment = y; }
    float& GetVerticalAdjustment() { return verticalAdjustment; }

    void Render(ID3D11DeviceContext* dc, Camera* camera) override
    {
        if (!primitive) return;

        // 1. Setup Warna (Sama)
        DirectX::XMFLOAT3 currentRGB = backColor;
        float textOffsetY = 0.0f;

        if (currentState == ButtonState::HOVER) { currentRGB = hoverColor; }
        else if (currentState == ButtonState::PRESSED) { currentRGB = pressColor; textOffsetY = 2.0f; }

        // 2. Gambar Primitif (Sama)
        primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f, currentRGB.x, currentRGB.y, currentRGB.z, 1.0f);
        primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f, borderColor.x, borderColor.y, borderColor.z, 0.5f);
        primitive->Render(dc);

        // 3. Render Text (LOGIKA ALIGNMENT BARU)
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            DirectX::XMFLOAT2 textSize = font->MeasureText(labelText, textScale);

            // LOGIKA X (SAMA SEPERTI SEBELUMNYA)
            float textX = 0.0f;
            switch (alignment)
            {
            case TextAlignment::Left:   textX = (float)rect.left + paddingX; break;
            case TextAlignment::Right:  textX = (float)rect.left + width - textSize.x - paddingX; break;
            case TextAlignment::Center:
            default:                    textX = (float)rect.left + (width - textSize.x) / 2.0f; break;
            }

            // --- LOGIKA Y (DIUPDATE) ---
            // Rumus Dasar Center Y
            float textY = (float)rect.top + (height - textSize.y) / 2.0f;

            // Tambahkan Efek Tombol Ditekan (State Offset)
            textY += textOffsetY;

            // Tambahkan Koreksi Manual (Visual Correction) <-- INI KUNCINYA
            textY += verticalAdjustment;
            // ---------------------------

            font->Draw(labelText.c_str(), textX, textY, textScale, 1, 1, 1, 1);
        }
    }

    // Di dalam class UIButtonPrimitive, bagian Public:
    DirectX::XMFLOAT3& GetColorNormal() { return backColor; }
    DirectX::XMFLOAT3& GetColorHover() { return hoverColor; }
    DirectX::XMFLOAT3& GetColorPress() { return pressColor; }
    DirectX::XMFLOAT3& GetColorBorder() { return borderColor; }

private:
    Primitive* primitive;
    std::string labelText;

    // Settings Baru
    float textScale = 0.625f;
    float paddingX = 10.0f;           // Default padding 10px
    TextAlignment alignment = TextAlignment::Center; // Default Center
    float verticalAdjustment = 3.0f;

    DirectX::XMFLOAT3 backColor, hoverColor, pressColor, borderColor;
};