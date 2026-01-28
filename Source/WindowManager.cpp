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
        // Skip debug window (walaupun nullptr tetap aman)
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

    // Force Debug Window (Kodingan lama, dibiarkan aman karena debugWindow nullptr)
    if (debugWindow && debugWindow->IsVisible())
    {
        SetWindowPos(debugWindow->GetHWND(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;

    // 1. UPDATE DATA IMGUI
    // Panggil DrawGUI agar tombol/menu dimasukkan ke antrian render ImGui
    scene->DrawGUI();

    // Cek Scene
    bool isBeyondScene = (dynamic_cast<SceneGameBeyond*>(scene) != nullptr);

    // 2. RENDER WINDOW (Sekarang cuma ada 1 Main Window)
    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;

        // [SET BACKGROUND COLOR]
        if (isBeyondScene) {
            win->BeginRender(0.1f, 0.1f, 0.15f); // Abu-abu kebiruan
        }
        else {
            win->BeginRender(0.0f, 0.0f, 0.0f); // Hitam pekat
        }

        // A. RENDER GAME SCENE 3D
        scene->OnResize(win->GetWidth(), win->GetHeight());
        scene->Render(dt, win->GetCamera());

        // B. [PENTING] RENDER IMGUI SEBAGAI OVERLAY
        // Ini kuncinya! Kita gambar UI di atas Scene 3D yang sudah digambar sebelumnya.
        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());

        // C. SWAP BUFFERS (VSync Nyala = 1)
        // Kita nyalakan VSync di sini karena ini sekarang Window Utama
        win->EndRender(1);
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
}

void WindowManager::ClearAll()
{
    std::lock_guard<std::mutex> lock(m_windowsMutex);
    windows.clear();
}