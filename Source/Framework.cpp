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
    Graphics::Instance().Initialize();

    // 1. Buat Main Window (Background)
    auto mainWin = WindowManager::Instance().CreateGameWindow("Main Game Window", 1920, 1080);
    // Input System tetap pakai Main Window (untuk game logic)
    Input::Instance().Initialize(mainWin->GetHWND());

    // ------------------------------------------------------------
    // 2. [FIX] BUAT DEDICATED DEBUG WINDOW LEBIH AWAL
    // ------------------------------------------------------------

    // UBAH UKURAN DI SINI (Contoh: Lebar 800, Tinggi 600)
    int debugW = 400;
    int debugH = 600;
    auto debugWin = WindowManager::Instance().CreateGameWindow("DEBUG CONSOLE", debugW, debugH);

    debugWin->SetPriority(0);
    WindowManager::Instance().SetDebugWindow(debugWin);

    // [BARU] ATUR POSISI WINDOW DI SINI (X, Y)
    // Contoh: Pojok Kiri Atas (X=50, Y=50)
    // Kamu bisa ganti angka 50, 50 dengan koordinat layar yang kamu mau.
    SDL_SetWindowPosition(debugWin->GetSDLWindow(), 50, 50);


    // ------------------------------------------------------------
    // 3. [FIX] INIT IMGUI MENGGUNAKAN DEBUG WINDOW!
    // ------------------------------------------------------------
    // Ini penting agar skala koordinat mouse ImGui sesuai dengan ukuran window console (600x400)
    ImGuiRenderer::Initialize(debugWin->GetHWND(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

    // Load Resources
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    // Init Scene
    scene = std::make_unique<SceneGameBeyond>();
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

    // Update setiap 1 detik
    if (timeAccumulator >= 1.0f)
    {
        float fps = static_cast<float>(frames);
        std::ostringstream outs;
        outs.precision(6);

        // Format Teks Title Bar
        outs << "DEBUG CONSOLE | FPS: " << fps << " (" << (1000.0f / fps) << " ms)";

        // [UBAH TARGET KE DEBUG WINDOW]
        // Kita ambil pointer Debug Window dari Manager
        GameWindow* debugWin = WindowManager::Instance().GetDebugWindow();

        if (debugWin)
        {
            // Ubah judul window Debug Console
            SetWindowTextA(debugWin->GetHWND(), outs.str().c_str());
        }

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