#include "UIPause.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include "FontTTF.h"

void UIPause::Initialize()
{
    auto device{ Graphics::Instance().GetDevice() };

    m_pauseSprite = std::make_unique<Sprite>(device, "Data/Sprite/UI/Sprite_Pause.png");
    m_font = std::make_unique<FontTTF>();
    m_font->Initialize("Data/Font/PixelifySans-Bold.ttf", FONT_SIZE);
}

void UIPause::Render(ID3D11DeviceContext* dc) const
{
    // BUG PREVENTION: Defensive guard clause. 
    // If the UI is called to render before it initialized (or if initialization failed), 
    // this prevents a hard crash via null pointer dereference.
    if (!m_pauseSprite || !m_font) return;

    auto rs{ Graphics::Instance().GetRenderState() };

    // Setup pipeline state for 2D UI Transparency rendering
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // Render the Background Panel
    m_pauseSprite->Render(
        dc,
        PANEL_POS_X, PANEL_POS_Y, 0.0f,               // Target X, Y, Z (Pre-calculated)
        SPRITE_WIDTH, SPRITE_HEIGHT,                  // Target Width, Height
        0.0f, 0.0f,                                   // Source X, Y (Top-left)
        SPRITE_WIDTH, SPRITE_HEIGHT,                  // Source Width, Height
        0.0f,                                         // Angle
        1.0f, 1.0f, 1.0f, 1.0f                        
    );

    // Render the text
    m_font->Draw(
        m_pauseText,
        TEXT_POS_X,
        TEXT_POS_Y,
        1.0f,                                         // Scale
        { 1.0f, 1.0f, 1.0f, 1.0f }                    
    );
}