#include <memory>
#include <sstream>
#include <iostream> // [DEBUG]
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

    std::cout << "[Framework] CHECKPOINT 1: Init Graphics..." << std::endl;
    Graphics::Instance().Initialize();

    std::cout << "[Framework] CHECKPOINT 2: Creating Main Window..." << std::endl;
    // Window pertama yang dibuat otomatis jadi index 0
    WindowManager::Instance().CreateGameWindow("Main Game Window", 1920, 1080);

    GameWindow* mainWin = GetMainWindow();
    if (!mainWin || !mainWin->GetHWND())
    {
        std::cerr << "[Framework] CRITICAL ERROR: Main Window HWND is NULL!" << std::endl;
        MessageBoxA(NULL, "Main Window failed to init", "Error", MB_OK);
        exit(-1);
    }

    std::cout << "[Framework] CHECKPOINT 3: Init Input System..." << std::endl;
    Input::Instance().Initialize(mainWin->GetHWND());

    std::cout << "[Framework] CHECKPOINT 4: Init ImGui..." << std::endl;
    // Pastikan ini dipanggil SETELAH window siap
    ImGuiRenderer::Initialize(mainWin->GetHWND(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

    std::cout << "[Framework] CHECKPOINT 5: Load Fonts..." << std::endl;
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    std::cout << "[Framework] CHECKPOINT 6: Create SceneGameBeyond..." << std::endl;
    scene = std::make_unique<SceneTitle>();

    std::cout << "[Framework] Initialization Complete!" << std::endl;
}

// ... (Sisa fungsi Destructor, Update, Render, dll SAMA SEPERTI SEBELUMNYA) ...
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
            // Logic set camera scene transition...
            // (Simpan logika camera set yang lama di sini)
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
        outs << "Main Game Window - FPS: " << fps;
        GameWindow* mainWin = GetMainWindow();
        if (mainWin) SetWindowTextA(mainWin->GetHWND(), outs.str().c_str());
        frames = 0;
        timeAccumulator -= 1.0f;
    }
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Ini mungkin tidak dipanggil secara otomatis lagi oleh Windows karena kita pakai Hook di GameWindow.
    // Tapi kita simpan sebagai helper jika diperlukan.
    GameWindow* mainWin = GetMainWindow();
    if (mainWin && hWnd == mainWin->GetHWND())
    {
        if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;
    }
    return 0;
}