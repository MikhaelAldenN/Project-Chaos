#pragma execution_character_set("utf-8")

#include "NaviPhaseTitle.h"
#include "NaviBoss.h"
#include "Boss_Phase01.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include "WindowManager.h"
#include "Player.h"
#include <SDL3/SDL.h>
#include <Windows.h>

// ============================================================
// Constructor
// ============================================================
NaviPhaseTitle::NaviPhaseTitle(Player* player)
    : m_player(player)
{
}

// ============================================================
// Enter  - Called once when this phase becomes active.
// ============================================================
void NaviPhaseTitle::Enter(NaviBoss* boss)
{
    // Build the title sprite from disk.
    auto device = Graphics::Instance().GetDevice();

    // --- Resize main window to bordered fullscreen ---
    {
        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);

        Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
        if (mainWindow && mainWindow->GetSDLWindow())
        {
            SDL_Window* sdlWin = mainWindow->GetSDLWindow();
            SDL_SetWindowBordered(sdlWin, true);      // Bordered (bukan borderless)
            SDL_SetWindowResizable(sdlWin, false);
            SDL_SetWindowAlwaysOnTop(sdlWin, false);
            SDL_SetWindowPosition(sdlWin, 0, 0);
            SDL_SetWindowSize(sdlWin, screenW, screenH); // Full-screen resolution
            mainWindow->SetPriority(50);
            WindowManager::Instance().MarkPriorityDirty();
        }
    }

    // Hide the boss visuals while the title screen is up.
    if (boss)
    {
        boss->SetFaceSpriteVisible(false);
    }

    // Disable player input during the title screen.
    if (m_player)
    {
        m_player->SetInputEnabled(false);
    }

    m_alpha = 0.0f;
    m_ready = false;
}

// ============================================================
// Exit  - Clean up before switching to the next phase.
// ============================================================
void NaviPhaseTitle::Exit(NaviBoss* boss)
{
    m_titleSprite.reset();
}

// ============================================================
// Update
// ============================================================
void NaviPhaseTitle::Update(float dt, NaviBoss* boss)
{
    if (!boss) return;

    // Fade-in
    if (m_alpha < 1.0f)
    {
        m_alpha += dt * m_fadeInSpeed;
        if (m_alpha >= 1.0f)
        {
            m_alpha = 1.0f;
            m_ready = true;
        }
    }

    // Wait for Enter key after the image has fully appeared.
    if (m_ready)
    {
        const bool* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState && keyState[SDL_SCANCODE_RETURN])
        {
            // Re-enable player input before handing off.
            if (m_player)
            {
                m_player->SetInputEnabled(true);
            }

            boss->ChangePhase(std::make_unique<Boss_Phase01>(m_player));
        }
    }
}

// ============================================================
// Render
// ============================================================
void NaviPhaseTitle::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss)
{
    if (!m_titleSprite || !context) return;

    // Read the current viewport to get the screen dimensions.
    D3D11_VIEWPORT vp;
    UINT numVP = 1;
    context->RSGetViewports(&numVP, &vp);

    const float screenW = vp.Width;
    const float screenH = vp.Height;

    // Draw the sprite centred, filling the whole screen.
    // Sprite::Render(dc, dx, dy, dz, dw, dh, angle, r, g, b, a)
    m_titleSprite->Render(
        context,
        0.0f, 0.0f,      // top-left position
        0.0f,            // depth (front-most)
        screenW, screenH, // fill the screen
        0.0f,            // no rotation
        1.0f, 1.0f, 1.0f, m_alpha // fade-in alpha
    );
}