#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h" // [WAJIB] Tambahkan ini untuk ambil DeviceContext

// Hapus include <imgui.h> di sini karena kita pakai wrapper ImGuiRenderer
// #include <imgui.h> 

void WindowManager::Update(float dt)
{
    EnforceWindowPriorities();
}

void WindowManager::EnforceWindowPriorities()
{
    std::vector<GameWindow*> sortedWindows;

    // 1. Ambil window high priority (< 100)
    for (auto& win : windows)
    {
        if (win->GetPriority() < 100)
        {
            sortedWindows.push_back(win.get());
        }
    }

    if (sortedWindows.empty()) return;

    // 2. SORT ASCENDING
    std::sort(sortedWindows.begin(), sortedWindows.end(),
        [](GameWindow* a, GameWindow* b) {
            return a->GetPriority() < b->GetPriority();
        });

    // 3. RELATIVE CHAINING
    HWND hInsertAfter = HWND_TOPMOST;

    for (GameWindow* win : sortedWindows)
    {
        SetWindowPos(win->GetHWND(),
            hInsertAfter,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        hInsertAfter = win->GetHWND();
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;

    // -------------------------------------------------------------------------
    // 1. UPDATE GUI CONTENT
    // -------------------------------------------------------------------------
    scene->DrawGUI();

    // -------------------------------------------------------------------------
    // 2. RENDER SEMUA WINDOW
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < windows.size(); ++i)
    {
        auto& win = windows[i];

        // A. Siapkan Layar
        win->BeginRender(0.5f, 0.5f, 0.5f);

        // B. Render Scene Game
        scene->OnResize(win->GetWidth(), win->GetHeight());
        scene->Render(dt, win->GetCamera());

        // C. RENDER IMGUI (HANYA DI MAIN WINDOW)
        if (i == 0)
        {
            // [FIX] Ambil Context dari Graphics system
            auto context = Graphics::Instance().GetDeviceContext();

            // Panggil Render milik ImGuiRenderer dengan context
            // Wrapper ini sudah melakukan ImGui::Render() di dalamnya, jadi aman.
            ImGuiRenderer::Render(context);
        }

        // D. Tampilkan
        win->EndRender();
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