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
#include "SceneIntro.h"
#include "SceneGameBreaker.h"

Framework* Framework::pInstance = nullptr;

Framework::Framework()
{
    pInstance = this;

    Graphics::Instance().Initialize();
    mainWindow = std::make_unique<GameWindow>("Main Game Window", 1920, 1080);

    Input::Instance().Initialize(mainWindow->GetHWND());
    ImGuiRenderer::Initialize(mainWindow->GetHWND(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

    // 1. Inisialisasi scene awal menjadi SceneTitle (bukan SceneGame)
    scene = std::make_unique<SceneIntro>();

    // 2. Set kamera awal berdasarkan SceneTitle
    if (auto introScene = dynamic_cast<SceneIntro*>(scene.get()))
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
    // [BARU] Cek apakah ada request ganti scene?
    if (nextScene)
    {
        // std::move akan otomatis men-delete scene lama dan memindahkan ownership scene baru
        scene = std::move(nextScene);

        // Reset logika kamera jika diperlukan (tergantung scene baru)
        if (auto gameScene = dynamic_cast<SceneGame*>(scene.get()))
        {
            mainWindow->SetCamera(gameScene->GetMainCamera());
        }
        else if (auto breakerScene = dynamic_cast<SceneGameBreaker*>(scene.get()))
        {
            mainWindow->SetCamera(breakerScene->GetMainCamera());
        }
        // Jika Title scene punya kamera sendiri, kita bisa set di sini atau biarkan default
        else if (auto titleScene = dynamic_cast<SceneTitle*>(scene.get()))
        {
            // Opsional: Set kamera title ke window utama jika perlu
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
        // 1. Tentukan warna default 
        float bgR = 0.0f;
        float bgG = 0.0f;
        float bgB = 0.0f;

        // 2. Cek apakah scene yang aktif sekarang adalah SceneIntro?
        // Jika IYA, ubah warna background jadi HITAM (0,0,0)
        if (dynamic_cast<SceneIntro*>(scene.get()))
        {
            bgR = 0.0f;
            bgG = 0.0f;
            bgB = 0.0f;
        }

        // 3. Masukkan variabel warna ke BeginRender
        mainWindow->BeginRender(bgR, bgG, bgB);
        if (scene)
        {
            scene->OnResize(mainWindow->GetWidth(), mainWindow->GetHeight());
            scene->Render(elapsedTime, mainWindow->GetCamera());
            scene->DrawGUI();
        }

        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
        mainWindow->EndRender(1); // Sync interval 1
    }

    WindowManager::Instance().RenderAll(elapsedTime, scene.get());
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