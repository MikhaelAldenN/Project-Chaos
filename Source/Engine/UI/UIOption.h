#pragma once

#include <cstdint>

// Forward declarations 
struct ID3D11DeviceContext;
class Primitive;

class UIOption
{
public:
    UIOption() = default;
    ~UIOption() = default;

    // --- Rule of 5 ---
    // Explicitly delete copy semantics to prevent expensive accidental copying of UI states
    UIOption(const UIOption&) = delete;
    UIOption& operator=(const UIOption&) = delete;
    UIOption(UIOption&&) = default;
    UIOption& operator=(UIOption&&) = default;

    void Initialize(Primitive* primitive) noexcept;

    void Render(ID3D11DeviceContext* dc, float alpha = 1.0f) const noexcept;

private:
    Primitive* m_primitive{ nullptr };

    static constexpr float SCREEN_WIDTH{ 1920.0f };
    static constexpr float SCREEN_HEIGHT{ 1080.0f };

    static constexpr float PANEL_WIDTH{ 800.0f };
    static constexpr float PANEL_HEIGHT{ 400.0f };

    static constexpr float PANEL_POS_X{ (SCREEN_WIDTH - PANEL_WIDTH) * 0.5f };
    static constexpr float PANEL_POS_Y{ (SCREEN_HEIGHT - PANEL_HEIGHT) * 0.5f };

    // The thickness of the white outline
    static constexpr float BORDER_THICKNESS{ 4.0f };

    // --- Colors ---
    // White outline
    static constexpr float OUTLINE_R{ 0.75f };
    static constexpr float OUTLINE_G{ 0.75f };
    static constexpr float OUTLINE_B{ 0.75f };

	// Dark translucent background
    static constexpr float BG_R{ 0.05f };
    static constexpr float BG_G{ 0.05f };
    static constexpr float BG_B{ 0.05f };
    static constexpr float BG_ALPHA_MULTIPLIER{ 0.35f }; 
};