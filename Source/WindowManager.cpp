#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h"
#include "SceneGameBeyond.h"
#include <mutex>

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

    // [PERBAIKAN] Cek apakah scene saat ini adalah SceneGameBeyond
    // Jangan lupa #include "SceneGameBeyond.h" di bagian atas file jika belum ada
    bool isBeyondScene = (dynamic_cast<SceneGameBeyond*>(scene) != nullptr);

    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;

        bool isMaster = (win.get() == debugWindow);

        if (isMaster)
        {
            // Debug Window tetap gelap (hitam/dark gray)
            win->BeginRender(0.1f, 0.1f, 0.1f);
            ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
            win->EndRender(1);
            vSyncTriggered = true;
        }
        else
        {
            // [LOGIKA BARU] Tentukan warna berdasarkan scene
            if (isBeyondScene) {
                // Abu-abu gelap (0.3f) khusus untuk SceneGameBeyond
                win->BeginRender(0.1f, 0.1f, 0.15f);
            }
            else {
                // Hitam murni (0.0f) untuk scene lainnya
                win->BeginRender(0.0f, 0.0f, 0.0f);
            }

            scene->OnResize(win->GetWidth(), win->GetHeight());
            scene->Render(dt, win->GetCamera());
            win->EndRender(0);
        }
    }

    // Failsafe VSync logic...
    if (!vSyncTriggered)
    {
        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
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
    std::lock_guard<std::mutex> lock(m_windowsMutex); // [BARU]

    auto newWindow = std::make_unique<GameWindow>(title, width, height);
    GameWindow* ptr = newWindow.get();
    windows.push_back(std::move(newWindow));
    return ptr;
}

void WindowManager::DestroyWindow(GameWindow* targetWindow)
{
    std::lock_guard<std::mutex> lock(m_windowsMutex); // [BARU]

    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [targetWindow](const std::unique_ptr<GameWindow>& p) {
                return p.get() == targetWindow;
            }),
        windows.end());
}

void WindowManager::ClearAll()
{
    std::lock_guard<std::mutex> lock(m_windowsMutex); // [BARU]
    windows.clear();
}