#include "SceneIntroBios.h"
#include "SceneIntroOS.h"
#include "Framework.h"
#include "System/Input.h"
#include "System/Graphics.h"

// --- KONFIGURASI BIOS ---
namespace ConfigBios {
    constexpr float FONT_SIZE = 0.625f;
    const float COLOR_YELLOW[4] = { 0.96f, 0.80f, 0.23f, 1.0f };
    const float COLOR_RED[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

    constexpr float FINISH_DELAY = 0.5f;
    constexpr float EXIT_DELAY = 1.5f;
}

SceneIntroBios::SceneIntroBios()
{
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);

    // Load Logo
    spriteLogoBoot = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Intro/Sprite_OrangSoloItu.png");

    SetupBiosLog();
}

void SceneIntroBios::SetupBiosLog()
{
    biosLogSystem = std::make_unique<Typewriter>();
    using namespace ConfigBios;

    biosLogSystem->AddLine(
        "SURAKARTAN - SYSTEM BOOT LOG \n\nBEYOND Modular BIOS v9.70BB, An Energy Star Ally \nCopyright (C) 1987-2026, Software Chaos, Inc. ",
        406.5f, 57.4f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "Version YN7K75\nNEURAL-LINK CPU at 444MHz",
        298.0f, 175.74f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddMemoryTestLine(
        "Memory Test : ", 0, 262144, 2.0f, "KB OK",
        740.0f, 175.74f + 23.5f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "(Reference: \"640K ought to be enough for anybody\")",
        298.0f, 235.0f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine(
        "Chaos Plug and Play BIOS Extension v1.0A\nCopyright (C) 2026, Software Chaos, Inc.",
        298.0f, 296.79f, FONT_SIZE, COLOR_YELLOW
    );

    biosLogSystem->AddLine("IDE Primary Master    ... [ST320411A]", 334.7f, 344.31f, FONT_SIZE, COLOR_YELLOW, 1.0f);
    biosLogSystem->AddLine("IDE Primary Slave     ... NONE", 334.7f, 368.5f, FONT_SIZE, COLOR_YELLOW);
    biosLogSystem->AddLine("IDE Secondary Master  ... [ECC DVD-ROM]\nIDE Secondary Slave   ... [!] WARNING: ANOMALY_DETECTED", 334.7f, 392.5f, FONT_SIZE, COLOR_YELLOW);

    biosLogSystem->AddLine(
        "[CRITICAL ALERT] Sector 0x004 is bleeding.\nThe blocks are no longer static. The Wall is thinner than you think.",
        298.7f, 560.0f, FONT_SIZE, COLOR_YELLOW
    );
}

void SceneIntroBios::Update(float elapsedTime)
{
    if (isExiting)
    {
        exitTimer += elapsedTime;
        if (exitTimer >= ConfigBios::EXIT_DELAY)
        {
            Framework::Instance()->ChangeScene(std::make_unique<SceneIntroOS>());
        }
        return;
    }

    if (biosLogSystem)
    {
        biosLogSystem->Update(elapsedTime);
        if (biosLogSystem->IsFinished())
        {
            finishDelayTimer += elapsedTime;
            if (finishDelayTimer >= ConfigBios::FINISH_DELAY)
            {
                isExiting = true;
            }
        }
    }

    if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
    {
        if (biosLogSystem && !biosLogSystem->IsFinished())
        {
            biosLogSystem->SkipCurrentLine();
        }
        else
        {
            isExiting = true;
        }
    }
}

void SceneIntroBios::Render(float dt, Camera* targetCamera)
{
    if (isExiting) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), blendFactor, 0xFFFFFFFF);
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");

    if (spriteLogoBoot)
    {
        spriteLogoBoot->Render(dc,
            298.0f, 57.4f, 0.0f,    // X, Y, Z
            93.0f, 93.0f,           // W, H
            0.0f,                   // Angle
            1.0f, 1.0f, 1.0f, 1.0f  // Color
        );
    }

    if (text)
    {
        text->Draw(
            "Press DEL to enter SETUP Press F1 to ESCAPE THE GRID\n09/01/2026-BB-586B-W877-2A5LEF09C-00",
            298.7f, 973.5f,
            ConfigBios::FONT_SIZE,
            ConfigBios::COLOR_YELLOW[0], ConfigBios::COLOR_YELLOW[1], ConfigBios::COLOR_YELLOW[2], ConfigBios::COLOR_YELLOW[3]
        );
    }

    if (biosLogSystem && text)
    {
        biosLogSystem->Render(text);
    }
}

void SceneIntroBios::OnResize(int width, int height)
{
    if (camera && height > 0)
    {
        camera->SetAspectRatio((float)width / (float)height);
    }
}

void SceneIntroBios::DrawGUI() {}