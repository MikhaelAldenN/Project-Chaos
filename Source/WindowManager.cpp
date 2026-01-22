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

    // Variable untuk melacak apakah kita sudah menutup frame ImGui
    bool imguiRendered = false;

    // 2. RENDER SEMUA WINDOW
    for (size_t i = 0; i < windows.size(); ++i)
    {
        auto& win = windows[i];

        // Skip jika sembunyi
        if (!win->IsVisible()) continue;

        // -----------------------------------------------------------
        // KASUS A: DEBUG WINDOW (RENDER IMGUI)
        // -----------------------------------------------------------
        if (win.get() == debugWindow)
        {
            win->BeginRender(0.1f, 0.1f, 0.1f);

            // Render ImGui di sini dengan Context yang benar
            auto context = Graphics::Instance().GetDeviceContext();
            ImGuiRenderer::Render(context);

            win->EndRender();

            // Tandai bahwa kita sudah merender ImGui frame ini
            imguiRendered = true;
            continue;
        }

        // -----------------------------------------------------------
        // KASUS B: WINDOW GAME BIASA
        // -----------------------------------------------------------
        win->BeginRender(0.5f, 0.5f, 0.5f);
        scene->OnResize(win->GetWidth(), win->GetHeight());
        scene->Render(dt, win->GetCamera());
        win->EndRender();
    }

    // [FIX ASSERTION]
    // Jika karena suatu alasan ImGui belum dirender (misal Debug Window hidden),
    // kita TETAP HARUS memanggil Render() secara internal untuk menutup frame.
    // Kalau tidak, ImGui akan crash di frame berikutnya.
    if (!imguiRendered)
    {
		auto context = Graphics::Instance().GetDeviceContext();
        ImGuiRenderer::Render(context);
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