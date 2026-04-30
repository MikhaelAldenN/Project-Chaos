#include "Framework.h"


Framework* Framework::pInstance = nullptr;

Framework::Framework()
{
    pInstance = this;
    Graphics::Instance().Initialize();

    if (!AudioManager::Instance().Initialize()) {  }

    // Buat Main Window (Fullscreen Borderless)
    auto mainWin = WindowManager::Instance().CreateGameWindow("Main Window (close here)", 1600, 900);
    mainWin->SetPriority(0);

    mainWin->SetVisible(true);
    // Tambahkan flag Resizable agar bisa di-drag ujungnya
    SDL_SetWindowResizable(mainWin->GetSDLWindow(), true);
    SDL_SetWindowBordered(mainWin->GetSDLWindow(), true);
    //SDL_SetWindowPosition(mainWin->GetSDLWindow(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowPosition(mainWin->GetSDLWindow(), 5, 35);

    // Posisikan di tengah saat awal


    Input::Instance().Initialize(mainWin->GetNativeHandle());

    // ------------------------------------------------------------
    // [UBAH] INIT IMGUI MENGGUNAKAN MAIN WINDOW!
    // ------------------------------------------------------------
    ImGuiRenderer::Initialize(mainWin->GetNativeHandle(), Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

    // Load Resources
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    // Init Scene
    scene = std::make_unique<SceneGame>();
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

Beyond::Window* Framework::GetMainWindow() const
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
        Beyond::Window* mainWin = GetMainWindow();
        if (mainWin && scene)
        {
        }
    }

    CalculateFrameStats(elapsedTime);
    Input::Instance().Update();
    AudioManager::Instance().Update(elapsedTime);

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
        //float fps = static_cast<float>(frames);
        //std::ostringstream outs;
        //outs.precision(6);

        //// Menggunakan judul resmi game barumu
        //outs << "FPS: " << fps << " (" << (1000.0f / fps) << " ms)";

        // Loop melalui semua window yang ada di WindowManager
        //size_t index = 0;
        //while (Beyond::Window* win = WindowManager::Instance().GetWindowByIndex(index))
        //{
        //    // Hanya update title pada window yang tidak disembunyikan (visible)
        //    if (win->IsVisible() && win->GetSDLWindow())
        //    {
        //        SDL_SetWindowTitle(win->GetSDLWindow(), outs.str().c_str());
        //    }
        //    index++;
        //}

        frames = 0;
        timeAccumulator -= 1.0f;
    }
}

void Framework::Quit()
{
    PostQuitMessage(0);
    SDL_Event quitEvent;
    SDL_zero(quitEvent);
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Beyond::Window* mainWin = GetMainWindow();
    if (mainWin && hWnd == mainWin->GetNativeHandle())
    {
        if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;
    }
    return 0;
}
void Framework::OnSubWindowClosed(Uint32 sdlWindowID)
{
    // Casting ke SceneBoss untuk mengakses fungsi spesifiknya
    SceneBoss* boss = dynamic_cast<SceneBoss*>(scene.get());
    if (boss) {
        boss->CloseSubWindowBySDLID(sdlWindowID);
    }
}
