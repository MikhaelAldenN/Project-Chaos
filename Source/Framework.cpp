#include <memory>
#include <sstream>
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

Framework* Framework::pInstance = nullptr;

Framework::Framework()
{
    pInstance = this;

    Graphics::Instance().Initialize();
    mainWindow = std::make_unique<GameWindow>("Main Game Window", 1920, 1080);

    Input::Instance().Initialize(mainWindow->GetHWND());
    ImGuiRenderer::Initialize(mainWindow->GetHWND(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    // Initialize starting scene (SceneTitle)
    scene = std::make_unique<SceneTitle>();

    // Set initial camera if applicable
    if (auto introScene = dynamic_cast<SceneIntroBios*>(scene.get()))
    {
        mainWindow->SetCamera(introScene->GetCamera());
    }
}

Framework::~Framework()
{
    scene.reset();
    ImGuiRenderer::Finalize();
    mainWindow.reset();
    pInstance = nullptr;
}

Framework* Framework::Instance()
{
    return pInstance;
}

void Framework::ChangeScene(std::unique_ptr<Scene> newScene)
{
    nextScene = std::move(newScene);
}

void Framework::Update(float elapsedTime)
{
    // Handle scene transition requests
    if (nextScene)
    {
        // Move ownership (automatically deletes the old scene)
        scene = std::move(nextScene);

        // Update the main window's camera reference based on the new scene type
        if (auto gameScene = dynamic_cast<SceneGame*>(scene.get()))
        {
            mainWindow->SetCamera(gameScene->GetMainCamera());
        }
        else if (auto breakerScene = dynamic_cast<SceneGameBreaker*>(scene.get()))
        {
            mainWindow->SetCamera(breakerScene->GetMainCamera());
        }
        else if (auto titleScene = dynamic_cast<SceneTitle*>(scene.get()))
        {
            mainWindow->SetCamera(titleScene->GetCamera());
        }
    }

    CalculateFrameStats(elapsedTime);
    Input::Instance().Update();
    ImGuiRenderer::NewFrame();

    if (scene)
    {
        scene->Update(elapsedTime);
    }
}

void Framework::Render(float elapsedTime)
{
    if (mainWindow)
    {
        // Determine background color (Black for Intro scenes, Gray for others)
        float bgR = 0.0f, bgG = 0.0f, bgB = 0.0f;

        if (dynamic_cast<SceneIntroBios*>(scene.get()) || dynamic_cast<SceneGameBreaker*>(scene.get()))
        {
            bgR = 0.5f; bgG = 0.5f; bgB = 0.5f;
        }

        // Render Main Window
        mainWindow->BeginRender(bgR, bgG, bgB);
        if (scene)
        {
            scene->OnResize(mainWindow->GetWidth(), mainWindow->GetHeight());
            scene->Render(elapsedTime, mainWindow->GetCamera());
            scene->DrawGUI();
        }

        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
        mainWindow->EndRender(1); // VSync On
    }

    // Render Sub-windows
    WindowManager::Instance().RenderAll(elapsedTime, scene.get());
}

void Framework::ForceUpdateRender()
{
    // Calculate delta time manually since the main loop is paused by message processing
    static Uint64 lastTime = 0;
    if (lastTime == 0) lastTime = SDL_GetPerformanceCounter();

    Uint64 currentTime = SDL_GetPerformanceCounter();
    float dt = (float)(currentTime - lastTime) / (float)SDL_GetPerformanceFrequency();
    lastTime = currentTime;

    // Cap delta time to prevent large jumps
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

    // Update window title every second
    if (timeAccumulator >= 1.0f)
    {
        float fps = static_cast<float>(frames);
        std::ostringstream outs;
        outs.precision(6);
        outs << "Main Game Window - FPS: " << fps << " / Frame Time: " << (1000.0f / fps) << " ms";

        if (mainWindow)
        {
            SetWindowTextA(mainWindow->GetHWND(), outs.str().c_str());
        }

        frames = 0;
        timeAccumulator -= 1.0f;
    }
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Pass input to ImGui first
    if (mainWindow && hWnd == mainWindow->GetHWND())
    {
        if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg)
    {
    case WM_SIZE:
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        if (mainWindow && hWnd == mainWindow->GetHWND())
            mainWindow->Resize(width, height);
        else
            WindowManager::Instance().HandleResize(hWnd, width, height);
        return 0;
    }

    return 0;
}