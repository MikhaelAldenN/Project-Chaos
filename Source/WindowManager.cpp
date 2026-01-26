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
    // Filter valid game windows
    std::vector<GameWindow*> sortedWindows;
    sortedWindows.reserve(windows.size());

    for (auto& win : windows)
    {
        if (win.get() != debugWindow && win->GetPriority() < 100)
        {
            sortedWindows.push_back(win.get());
        }
    }

    // Sort by priority (Ascending)
    std::sort(sortedWindows.begin(), sortedWindows.end(),
        [](GameWindow* a, GameWindow* b) {
            return a->GetPriority() < b->GetPriority();
        });

    // Apply Z-Order
    HWND hInsertAfter = HWND_TOPMOST;
    for (GameWindow* win : sortedWindows)
    {
        SetWindowPos(win->GetHWND(), hInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        hInsertAfter = win->GetHWND();
    }

    // Force Debug Window to be always on top
    if (debugWindow && debugWindow->IsVisible())
    {
        SetWindowPos(debugWindow->GetHWND(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;
    scene->DrawGUI();

    bool vSyncTriggered = false;

    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;

        bool isMaster = (win.get() == debugWindow);

        // Render Logic
        if (isMaster)
        {
            // Debug Window: Clears with dark color, renders ImGui, and WAITS for VSync (Interval 1)
            win->BeginRender(0.1f, 0.1f, 0.1f);
            ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
            win->EndRender(1);
            vSyncTriggered = true;
        }
        else
        {
            // Game Windows: Clears with gray, renders Scene, NO VSync wait (Interval 0)
            win->BeginRender(0.5f, 0.5f, 0.5f);
            scene->OnResize(win->GetWidth(), win->GetHeight());
            scene->Render(dt, win->GetCamera());
            win->EndRender(0);
        }
    }

    // Failsafe: If no VSync window was rendered (e.g. Debug is hidden), manually sleep
    // to prevent GPU usage from spiking to 100% (Infinite FPS)
    if (!vSyncTriggered)
    {
        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
        SDL_Delay(16); // ~60 FPS cap
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