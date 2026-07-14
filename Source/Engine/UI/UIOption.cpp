#include "UIOption.h"
#include "Primitive.h"
#include "System/Graphics.h" 

void UIOption::Initialize(Primitive* primitive) noexcept
{
    m_primitive = primitive;
}

void UIOption::Render(ID3D11DeviceContext* dc, float alpha) const noexcept
{
    // BUG PREVENTION: Defensive guard clause.
    if (!m_primitive || alpha <= 0.001f)
    {
        return;
    }

    // ANTICIPATION BUG GUARD: Explicitly enforce correct transparency states.
    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    // Draw Inner Translucent Background Layer 
    constexpr float innerX{ PANEL_POS_X + BORDER_THICKNESS };
    constexpr float innerY{ PANEL_POS_Y + BORDER_THICKNESS };
    constexpr float innerW{ PANEL_WIDTH - (BORDER_THICKNESS * 2.0f) };
    constexpr float innerH{ PANEL_HEIGHT - (BORDER_THICKNESS * 2.0f) };

    m_primitive->Rect(
        innerX, innerY, innerW, innerH,
        0.0f, 0.0f, 0.0f,
        BG_R, BG_G, BG_B,
        alpha * BG_ALPHA_MULTIPLIER
    );

    // Draw 4 Border Lines

    // Top Edge
    m_primitive->Rect(PANEL_POS_X, PANEL_POS_Y, PANEL_WIDTH, BORDER_THICKNESS, 0.0f, 0.0f, 0.0f, OUTLINE_R, OUTLINE_G, OUTLINE_B, alpha);

    // Bottom Edge
    m_primitive->Rect(PANEL_POS_X, PANEL_POS_Y + PANEL_HEIGHT - BORDER_THICKNESS, PANEL_WIDTH, BORDER_THICKNESS, 0.0f, 0.0f, 0.0f, OUTLINE_R, OUTLINE_G, OUTLINE_B, alpha);

    // Left Edge
    m_primitive->Rect(PANEL_POS_X, PANEL_POS_Y + BORDER_THICKNESS, BORDER_THICKNESS, PANEL_HEIGHT - (BORDER_THICKNESS * 2.0f), 0.0f, 0.0f, 0.0f, OUTLINE_R, OUTLINE_G, OUTLINE_B, alpha);

    // Right Edge
    m_primitive->Rect(PANEL_POS_X + PANEL_WIDTH - BORDER_THICKNESS, PANEL_POS_Y + BORDER_THICKNESS, BORDER_THICKNESS, PANEL_HEIGHT - (BORDER_THICKNESS * 2.0f), 0.0f, 0.0f, 0.0f, OUTLINE_R, OUTLINE_G, OUTLINE_B, alpha);

    // Batch draw down to the GPU layout context
    m_primitive->Render(dc);
}