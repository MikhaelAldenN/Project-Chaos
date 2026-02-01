#include <memory>
#include <sstream>
#include <iostream> 
#include <imgui.h>
#include <SDL3/SDL.h>

#include "Framework.h"
#include "System/Graphics.h"
#include "System/ImGuiRenderer.h"
#include "System/Input.h"
#include "WindowManager.h"
#include "SceneGame.h"
#include "SceneTitle.h"
#include "SceneIntroBios.h"
#include "SceneGameBreaker.h"
#include "SceneIntroOS.h"
#include "SceneGameBeyond.h"

Framework* Framework::pInstance = nullptr;

Framework::Framework()
{
    pInstance = this;
    Graphics::Instance().Initialize();

    // 1. Buat Main Window (Fullscreen Borderless)
    auto mainWin = WindowManager::Instance().CreateGameWindow("Main Game Window", 1920, 1080);
    mainWin->SetPriority(0);

    SDL_SetWindowBordered(mainWin->GetSDLWindow(), false);
    SDL_SetWindowPosition(mainWin->GetSDLWindow(), 0, 0);
    SDL_SetWindowSize(mainWin->GetSDLWindow(), GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    mainWin->SetVisible(true);

    Input::Instance().Initialize(mainWin->GetHWND());

    // ------------------------------------------------------------
    // [HAPUS] BAGIAN PEMBUATAN WINDOW DEBUG DIHAPUS
    // ------------------------------------------------------------
    /* int debugW = 400;
    int debugH = 600;
    auto debugWin = WindowManager::Instance().CreateGameWindow("DEBUG CONSOLE", debugW, debugH);
    debugWin->SetPriority(0);
    WindowManager::Instance().SetDebugWindow(debugWin);
    SDL_SetWindowPosition(debugWin->GetSDLWindow(), 50, 50);
    */

    // ------------------------------------------------------------
    // [UBAH] INIT IMGUI MENGGUNAKAN MAIN WINDOW!
    // ------------------------------------------------------------
    // Sekarang ImGui akan menempel pada Main Window, bukan Debug Window.
    ImGuiRenderer::Initialize(mainWin->GetHWND(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

    // Load Resources
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    // Init Scene
    scene = std::make_unique<SceneGameBreaker>();
}

Framework::~Framework()
{
    scene.reset();
    WindowManager::Instance().ClearAll();
    ImGuiRenderer::Finalize();
    pInstance = nullptr;
}

Framework* Framework::Instance() { return pInstance; }
void Framework::ChangeScene(std::unique_ptr<Scene> newScene) { nextScene = std::move(newScene); }

GameWindow* Framework::GetMainWindow() const
{
    return WindowManager::Instance().GetWindowByIndex(0);
}

void Framework::Render(float elapsedTime)
{
    WindowManager::Instance().RenderAll(elapsedTime, scene.get());
}

void Framework::Update(float elapsedTime)
{
    if (nextScene)
    {
        scene = std::move(nextScene);
        GameWindow* mainWin = GetMainWindow();
        if (mainWin && scene)
        {
            // Logic transition...
        }
    }

    CalculateFrameStats(elapsedTime);
    Input::Instance().Update();
    ImGuiRenderer::NewFrame();

    if (scene) scene->Update(elapsedTime);
}

void Framework::ForceUpdateRender()
{
    static Uint64 lastTime = 0;
    if (lastTime == 0) lastTime = SDL_GetPerformanceCounter();
    Uint64 currentTime = SDL_GetPerformanceCounter();
    float dt = (float)(currentTime - lastTime) / (float)SDL_GetPerformanceFrequency();
    lastTime = currentTime;
    if (dt > 0.05f) dt = 0.05f;
    Update(dt);
    Render(dt);
}

void Framework::CalculateFrameStats(float dt)
{
    static int frames = 0;
    static float timeAccumulator = 0.0f;

    frames++;
    timeAccumulator += dt;

    if (timeAccumulator >= 1.0f)
    {
        float fps = static_cast<float>(frames);
        std::ostringstream outs;
        outs.precision(6);

        // [UBAH] Set text ke Main Window karena Debug Window sudah tidak ada
        // (Meskipun fullscreen borderless biasanya tidak punya title bar, tapi tetap aman dipanggil)
        outs << "BEYOND BREAKER | FPS: " << fps << " (" << (1000.0f / fps) << " ms)";

        GameWindow* mainWin = GetMainWindow();
        if (mainWin)
        {
            SetWindowTextA(mainWin->GetHWND(), outs.str().c_str());
        }

        frames = 0;
        timeAccumulator -= 1.0f;
    }
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    GameWindow* mainWin = GetMainWindow();
    if (mainWin && hWnd == mainWin->GetHWND())
    {
        if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;
    }
    return 0;
}