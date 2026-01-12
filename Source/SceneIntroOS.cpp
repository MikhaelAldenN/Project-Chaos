#include "SceneIntroOS.h"
#include "SceneTitle.h"
#include "Framework.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "MathUtils.h" 

// --- KONFIGURASI CONSTANT ---
namespace ConfigOS {
    constexpr float SCREEN_WIDTH = 1920.0f;
    constexpr float SCREEN_HEIGHT = 1080.0f;
    constexpr float LOGO_WIDTH = 850.0f;
    constexpr float LOGO_HEIGHT = 105.0f;

    constexpr float ANIM_DURATION = 1.0f;
    constexpr float LOADING_DURATION = 5.0f;
    constexpr float BLINK_INTERVAL = 0.1f;

    // Layout
    constexpr float LOGO_CENTER_Y = 370.0f;
	constexpr float LOGO_OFFSET_Y = 3.0f; // Gap Each Logo
    constexpr float RECT_X = 304.0f;
    constexpr float RECT_Y = 48.0f;
    constexpr float RECT_W = 1312.0f;
    constexpr float RECT_H = 984.0f;

    // Text
    constexpr float FONT_SIZE = 0.625f;
    const float COLOR_BG[4] = { 0.96f, 0.80f, 0.23f, 1.0f };
    const float COLOR_TXT[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

SceneIntroOS::SceneIntroOS()
{
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(ConfigOS::SCREEN_WIDTH, ConfigOS::SCREEN_HEIGHT, 0.1f, 1000.0f);

    // Load Assets
    spriteOSLogo = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/sprite_Logo_BRICK-DOS.png");
    primitiveBatch = std::make_unique<Primitive>(Graphics::Instance().GetDevice());

    // Setup Animation Positions
    float screenCenter = ConfigOS::SCREEN_WIDTH * 0.5f;
    float logoHalf = ConfigOS::LOGO_WIDTH * 0.5f;

    logoTargetX = screenCenter - logoHalf;

    // Logo 1 (Atas - Dari Kiri)
    logo1_StartX = -ConfigOS::LOGO_WIDTH;
    logo1_CurrentX = logo1_StartX;
    logo1_Y = ConfigOS::LOGO_CENTER_Y;

    // Logo 2 (Bawah - Dari Kanan)
    logo2_StartX = ConfigOS::SCREEN_WIDTH;
    logo2_CurrentX = logo2_StartX;
    logo2_Y = ConfigOS::LOGO_CENTER_Y + ConfigOS::LOGO_OFFSET_Y;

    currentState = OSState::AnimatingLogo;
}

void SceneIntroOS::Update(float elapsedTime)
{
    timer += elapsedTime;
    blinkTimer += elapsedTime;

    if (blinkTimer > ConfigOS::BLINK_INTERVAL) blinkTimer = 0;

	// Animation State Machine
    switch (currentState)
    {
    case OSState::AnimatingLogo:
        animTimer += elapsedTime;

        if (animTimer < ConfigOS::ANIM_DURATION)
        {
            float t = animTimer / ConfigOS::ANIM_DURATION;
            logo1_CurrentX = MathUtils::Lerp(logo1_StartX, logoTargetX, t);
            logo2_CurrentX = MathUtils::Lerp(logo2_StartX, logoTargetX, t);
        }
        else
        {
            logo1_CurrentX = logoTargetX;
            logo2_CurrentX = logoTargetX;
            currentState = OSState::ShowText;
        }
        break;

    case OSState::ShowText:
        // Di sini logo sudah diam, kita hanya menunggu loading time
        // Jika ingin ada animasi text fade-in, bisa ditambahkan di sini
        break;

    case OSState::Finished:
        break;
    }

	// Scene Transition
    bool timeIsUp = timer >= ConfigOS::LOADING_DURATION;
    bool skipPressed = Input::Instance().GetKeyboard().IsTriggered(VK_RETURN);

    if (timeIsUp || skipPressed)
    {
        Framework::Instance()->ChangeScene(std::make_unique<SceneTitle>());
    }
}

void SceneIntroOS::Render(float dt, Camera* targetCamera)
{
    auto dc = Graphics::Instance().GetDeviceContext();
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), blendFactor, 0xFFFFFFFF);
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");

    // Render Background
    if (primitiveBatch)
    {
        primitiveBatch->Rect(
            ConfigOS::RECT_X, ConfigOS::RECT_Y,
            ConfigOS::RECT_W, ConfigOS::RECT_H,
            0.0f, 0.0f, 0.0f,
            ConfigOS::COLOR_BG[0], ConfigOS::COLOR_BG[1], ConfigOS::COLOR_BG[2], ConfigOS::COLOR_BG[3]
        );
        primitiveBatch->Render(dc);
    }

	// Render Logo
    if (spriteOSLogo)
    {
        spriteOSLogo->Render(dc,
            logo1_CurrentX, logo1_Y, 0.0f,              // X, Y, Z
            ConfigOS::LOGO_WIDTH, ConfigOS::LOGO_HEIGHT,// W, H
            0.0f,                                       // Angle
            1.0f, 1.0f, 1.0f, 1.0f                      // Color
        );

        spriteOSLogo->Render(dc,
            logo2_CurrentX, logo2_Y, 0.0f,              
            ConfigOS::LOGO_WIDTH, ConfigOS::LOGO_HEIGHT,
            0.0f,                                       
            1.0f, 1.0f, 1.0f, 1.0f                      
        );
    }

    // Render Text
    if (currentState == OSState::ShowText && text)
    {
        text->Draw("BRICK-DOS Opereating System\n        Version 1.2",
            733.5f, 593.0f, ConfigOS::FONT_SIZE,
            ConfigOS::COLOR_TXT[0], ConfigOS::COLOR_TXT[1], ConfigOS::COLOR_TXT[2], ConfigOS::COLOR_TXT[3]);

        text->Draw("Copyright (c) Comreba Corporation, 1987. All Rights Reserved.\n    BRICK-DOS is a registered trademark of Comreba Corp.",
            450.0f, 903.0f, ConfigOS::FONT_SIZE,
            ConfigOS::COLOR_TXT[0], ConfigOS::COLOR_TXT[1], ConfigOS::COLOR_TXT[2], ConfigOS::COLOR_TXT[3]);
    }
}

void SceneIntroOS::OnResize(int width, int height)
{
    if (camera) camera->SetAspectRatio((float)width / (float)height);
}

void SceneIntroOS::DrawGUI() {}