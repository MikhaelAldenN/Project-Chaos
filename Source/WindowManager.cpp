#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h"
#include "SceneGameBeyond.h"
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

    // [OPTIMISASI] Gunakan SWP_NOREDRAW untuk mengurangi flicker saat reordering
    UINT uFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW;

    for (GameWindow* win : sortedWindows)
    {
        SetWindowPos(win->GetHWND(), hInsertAfter, 0, 0, 0, 0, uFlags);
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

    // 1. UPDATE DATA IMGUI (Cukup sekali per frame)
    scene->DrawGUI();

    bool isBeyondScene = (dynamic_cast<SceneGameBeyond*>(scene) != nullptr);
    auto context = Graphics::Instance().GetDeviceContext();
    auto mainWindow = Framework::Instance()->GetMainWindow();

    // 2. RENDER WINDOW
    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;

        if (win.get() != mainWindow && !win->ShouldRender(dt))
        {
            continue;
        }

        // [OPTIMISASI KECIL] Warna background hitam murni lebih cepat diproses
        if (isBeyondScene) {
            win->BeginRender(0.1f, 0.1f, 0.15f);
        }
        else {
            win->BeginRender(0.0f, 0.0f, 0.0f);
        }

        scene->OnResize(win->GetWidth(), win->GetHeight());

        // Render Scene 
        // Note: Pastikan Scene::Render melakukan Frustum Culling! 
        // Jika kamera window melihat tembok kosong, jangan draw Boss-nya.
        scene->Render(dt, win->GetCamera());

        // Render ImGui HANYA di Main Window
        if (win.get() == mainWindow)
        {
            ImGuiRenderer::Render(context);
        }

        // [OPTIMISASI 2 - KRUSIAL]
        // Hanya Main Window yang boleh Sync Interval 1 (VSync).
        // Window pecahan (anak) harus 0 (Immediate) agar tidak saling menunggu.
        int syncInterval = (win.get() == mainWindow) ? 1 : 0;

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