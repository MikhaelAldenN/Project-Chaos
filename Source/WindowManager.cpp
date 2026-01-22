#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h"

void WindowManager::Update(float dt)
{
    EnforceWindowPriorities();
}

void WindowManager::EnforceWindowPriorities()
{
    std::vector<GameWindow*> sortedWindows;

    // 1. Ambil window game (KECUALIKAN DEBUG WINDOW)
    for (auto& win : windows)
    {
        // Debug window kita urus terpisah di bawah
        if (win.get() == debugWindow) continue;

        if (win->GetPriority() < 100)
        {
            sortedWindows.push_back(win.get());
        }
    }

    // 2. SORT WINDOW GAME (Ascending: 0 paling penting)
    if (!sortedWindows.empty())
    {
        std::sort(sortedWindows.begin(), sortedWindows.end(),
            [](GameWindow* a, GameWindow* b) {
                return a->GetPriority() < b->GetPriority();
            });

        // 3. Tumpuk Window Game
        // Kita mulai menumpuk window game SEDIKIT DI BAWAH Topmost murni
        // agar tidak berebut tempat dengan Debug Window.
        HWND hInsertAfter = HWND_TOPMOST;

        // Opsional: Kalau mau Debug Window benar-benar raja, 
        // Window game bisa kita taruh di HWND_NOTOPMOST dulu, 
        // tapi HWND_TOPMOST biasanya cukup aman karena OS memprioritaskan yang aktif.

        for (GameWindow* win : sortedWindows)
        {
            SetWindowPos(win->GetHWND(),
                hInsertAfter,
                0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            hInsertAfter = win->GetHWND();
        }
    }

    // -----------------------------------------------------------
    // [BARU] 4. PAKSA DEBUG WINDOW JADI RAJA (PALING ATAS)
    // -----------------------------------------------------------
    if (debugWindow && debugWindow->IsVisible())
    {
        // Kita paksa dia jadi HWND_TOPMOST setiap frame.
        // SWP_NOACTIVATE penting agar dia tidak mencuri fokus keyboard 
        // saat kita sedang main game, tapi tetap bisa diklik.
        SetWindowPos(debugWindow->GetHWND(),
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;

    // 1. UPDATE GUI CONTENT
    scene->DrawGUI();

    // Variable untuk melacak apakah VSync sudah dilakukan frame ini
    bool vSyncTriggered = false;

    // 2. RENDER SEMUA WINDOW
    for (size_t i = 0; i < windows.size(); ++i)
    {
        auto& win = windows[i];

        // Skip jika sembunyi
        if (!win->IsVisible()) continue;

        // A. TENTUKAN APAKAH INI WINDOW "KETUA" (DEBUG CONSOLE)
        // Jika ini Debug Window, kita aktifkan VSync (1) agar FPS terkunci di 60.
        // Window lain (Tracking/Lens) kita set 0 agar render secepat kilat.
        bool isMasterWindow = (win.get() == debugWindow);

        // -----------------------------------------------------------
        // RENDER LOGIC
        // -----------------------------------------------------------
        if (isMasterWindow)
        {
            // --- RENDER DEBUG WINDOW (BACKGROUND GELAP) ---
            win->BeginRender(0.1f, 0.1f, 0.1f);

            auto context = Graphics::Instance().GetDeviceContext();
            ImGuiRenderer::Render(context);

            // [PENTING] EndRender(1) artinya "Tunggu Monitor Refresh" (VSync ON)
            win->EndRender(1);
            vSyncTriggered = true; // Tandai kita sudah ngerem
        }
        else
        {
            // --- RENDER GAME WINDOW (SCENE 3D) ---
            win->BeginRender(0.5f, 0.5f, 0.5f);
            scene->OnResize(win->GetWidth(), win->GetHeight());
            scene->Render(dt, win->GetCamera());

            // [PENTING] EndRender(0) artinya "Jangan Tunggu, Langsung Tampil" (VSync OFF)
            // Ini aman KARENA kita sudah menunggu di Debug Window tadi.
            win->EndRender(0);
        }
    }

    // [SAFETY NET / REM DARURAT]
    // Jika Debug Window sedang disembunyikan/minimize, maka tidak ada yang ngerem (vSyncTriggered = false).
    // Akibatnya FPS akan loncat ke 2000+ lagi dan bikin layar hitam.
    // Kita harus ngerem manual pakai SDL_Delay.
    if (!vSyncTriggered)
    {
        // Render ImGui internal (untuk safety assertion)
        auto context = Graphics::Instance().GetDeviceContext();
        ImGuiRenderer::Render(context);

        // Tidur sebentar ~16ms (kira-kira 60 FPS) agar GPU bisa bernafas
        SDL_Delay(16);
    }
}

void WindowManager::HandleResize(HWND hWnd, int width, int height)
{
    for (auto& win : windows)
    {
        if (win->GetHWND() == hWnd)
        {
            win->Resize(width, height);
            return;
        }
    }
}

GameWindow* WindowManager::CreateGameWindow(const char* title, int width, int height)
{
    auto newWindow = std::make_unique<GameWindow>(title, width, height);
    GameWindow* ptr = newWindow.get();
    windows.push_back(std::move(newWindow));
    return ptr;
}

void WindowManager::DestroyWindow(GameWindow* targetWindow)
{
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [targetWindow](const std::unique_ptr<GameWindow>& p) {
                return p.get() == targetWindow;
            }),
        windows.end());
}

void WindowManager::ClearAll()
{
    windows.clear();
}