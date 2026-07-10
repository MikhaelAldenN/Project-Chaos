#pragma once

#include <memory>
#include <string>

// Forward declarations 
struct ID3D11DeviceContext;
class Sprite;
class FontTTF;

class UIPause
{
public:
    UIPause() = default;
    ~UIPause() = default;

    // Explicitly delete copy constructor and assignment operator to prevent 
    // accidental copying of unique_ptrs or expensive UI objects.
    UIPause(const UIPause&) = delete;
    UIPause& operator=(const UIPause&) = delete;
    UIPause(UIPause&&) = default;
    UIPause& operator=(UIPause&&) = default;

    void Initialize();
    void Render(ID3D11DeviceContext* dc) const;

private:
    // Core Components initialized to null via brace initialization
    std::unique_ptr<Sprite> m_pauseSprite{};
    std::unique_ptr<FontTTF> m_font{};

    // Cache the string to prevent `std::string` from allocating memory / utilizing SSO 
    // every single frame during the Render loop.
    const std::string m_pauseText{ "PAUSE" };

    // ========================================================================
    // CONSTEXPR CONFIGURATION 
    // ========================================================================
    static constexpr float SCREEN_WIDTH{ 1920.0f };
    static constexpr float SCREEN_HEIGHT{ 1080.0f };

    static constexpr float SPRITE_WIDTH{ 339.0f };
    static constexpr float SPRITE_HEIGHT{ 456.0f };

    static constexpr float PANEL_POS_X{ (SCREEN_WIDTH - SPRITE_WIDTH) * 0.5f };
    static constexpr float PANEL_POS_Y{ (SCREEN_HEIGHT - SPRITE_HEIGHT) * 0.5f };

    static constexpr float TEXT_POS_X{ PANEL_POS_X + 80.0f };
    static constexpr float TEXT_POS_Y{ PANEL_POS_Y + 86.0f };

    static constexpr float FONT_SIZE{ 60.0f };
};