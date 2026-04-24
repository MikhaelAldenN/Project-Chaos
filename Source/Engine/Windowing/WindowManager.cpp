#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h"
#include "SceneBoss.h"
#include <mutex>
#include <Framework.h>

void WindowManager::Update(float dt)
{
    // [OPTIMISASI 1] Hanya update Z-Order jika ditandai 'dirty'
    // Jangan panggil SetWindowPos setiap frame! OS akan ngelag.
    if (m_dirtyPriority)
    {
        EnforceWindowPriorities();
        m_dirtyPriority = false;
    }
}

void WindowManager::EnforceWindowPriorities()
{
    // 1. Filter valid game windows
    std::vector<GameWindow*> sortedWindows;
    sortedWindows.reserve(windows.size());

    for (auto& win : windows)
    {
        if (win.get() != debugWindow && win->GetPriority() < 100)
        {
            sortedWindows.push_back(win.get());
        }
    }

    // 2. Sort by priority (Ascending: Priority 0 paling atas, makin besar makin di belakang)
    // Note: Logika sort kamu sebelumnya Ascending, pastikan ini sesuai keinginanmu.
    std::sort(sortedWindows.begin(), sortedWindows.end(),
        [](GameWindow* a, GameWindow* b) {
            return a->GetPriority() < b->GetPriority();
        });

    // =========================================================
    // [MODIFIKASI] SMART TOPMOST
    // =========================================================
    HWND hInsertAfter = m_topmostEnabled ? HWND_TOPMOST : HWND_NOTOPMOST;
    // =========================================================

    // Gunakan flag SWP_NOACTIVATE agar tidak mencuri fokus keyboard
    UINT uFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW;

    for (GameWindow* win : sortedWindows)
    {
        SetWindowPos(win->GetHWND(), hInsertAfter, 0, 0, 0, 0, uFlags);

        // Chain Z-Order: Window berikutnya diletakkan DI BAWAH window ini.
        hInsertAfter = win->GetHWND();
    }

    if (debugWindow && debugWindow->IsVisible())
    {
        SetWindowPos(debugWindow->GetHWND(), HWND_TOPMOST, 0, 0, 0, 0, uFlags);
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;

    // 1. UPDATE DATA IMGUI
    scene->DrawGUI();

    bool isBeyondScene = (dynamic_cast<SceneBoss*>(scene) != nullptr);
    auto context = Graphics::Instance().GetDeviceContext();
    auto mainWindow = Framework::Instance()->GetMainWindow();

    // Flag VSync: Akan memastikan hanya SATU window per frame yang mengunci FPS
    bool vsyncApplied = false;

    // 2. RENDER WINDOW
    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;
        if (win.get() != mainWindow && !win->ShouldRender(dt)) continue;

        if (isBeyondScene) win->BeginRender(0.1f, 0.1f, 0.15f);
        else win->BeginRender(0.0f, 0.0f, 0.0f);

        scene->OnResize(win->GetWidth(), win->GetHeight());
        scene->Render(dt, win->GetCamera());

        if (win.get() == mainWindow) ImGuiRenderer::Render(context);

        // [THE FIX] Terapkan VSync (1) HANYA pada window pertama yang dirender.
        // Ini mengunci kecepatan update seluruh game (CPU & GPU) ke kecepatan monitor (60Hz).
        int syncInterval = 0;
        if (!vsyncApplied)
        {
            syncInterval = 1;
            vsyncApplied = true;
        }

        win->EndRender(syncInterval);
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
    std::lock_guard<std::mutex> lock(m_windowsMutex);

    auto newWindow = std::make_unique<GameWindow>(title, width, height);
    GameWindow* ptr = newWindow.get();
    windows.push_back(std::move(newWindow));
    return ptr;
}

void WindowManager::DestroyWindow(GameWindow* targetWindow)
{
    std::lock_guard<std::mutex> lock(m_windowsMutex);

    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [targetWindow](const std::unique_ptr<GameWindow>& p) {
                return p.get() == targetWindow;
            }),
        windows.end());

    // Trigger re-sort
    MarkPriorityDirty();
}

void WindowManager::ClearAll()
{
    std::lock_guard<std::mutex> lock(m_windowsMutex);
    windows.clear();
}