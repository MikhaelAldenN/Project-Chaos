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
    std::vector<Beyond::Window*> sortedWindows;
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
        [](Beyond::Window* a, Beyond::Window* b) {
            return a->GetPriority() < b->GetPriority();
        });

    // =========================================================
    // [MODIFIKASI] SMART TOPMOST
    // =========================================================
    HWND hInsertAfter = m_topmostEnabled ? HWND_TOPMOST : HWND_NOTOPMOST;
    // =========================================================

    // Gunakan flag SWP_NOACTIVATE agar tidak mencuri fokus keyboard
    UINT uFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW;

    for (Beyond::Window* win : sortedWindows)
    {
        SetWindowPos(win->GetNativeHandle(), hInsertAfter, 0, 0, 0, 0, uFlags);

        // Chain Z-Order: Window berikutnya diletakkan DI BAWAH window ini.
        hInsertAfter = win->GetNativeHandle();
    }

    if (debugWindow && debugWindow->IsVisible())
    {
        SetWindowPos(debugWindow->GetNativeHandle(), HWND_TOPMOST, 0, 0, 0, 0, uFlags);
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

        // DEBUG SEMENTARA
        if (win->IsTransparent())
        {
            Camera* cam = win->GetCamera();
            char dbg[128];
            sprintf_s(dbg, "[RENDER] TransparentWin camera ptr=%p\n", (void*)cam);
            OutputDebugStringA(dbg);
        }
        // END DEBUG

// =========================================================
        // [FIX] HACK ALPHA LAYERED WINDOW
        // Jangan pakai 0.0f! Pakai 0.01f (1% opacity) agar 
        // pixel di tengah window tidak di-ignore oleh hit-test OS.
        // =========================================================
        float clearAlpha = win->IsTransparent() ? 0.01f : 1.0f;

        if (win->IsTransparent())
        {
            win->BeginRender(0.0f, 0.0f, 0.0f, clearAlpha);
        }
        else
        {
            if (isBeyondScene) win->BeginRender(0.1f, 0.1f, 0.15f, clearAlpha);
            else win->BeginRender(0.0f, 0.0f, 0.0f, clearAlpha);
        }

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
        if (win->GetNativeHandle() == hWnd)
        {
            win->Resize(width, height);
            return;
        }
    }
}

Beyond::Window* WindowManager::CreateGameWindow(const char* title, int width, int height, bool isTransparent)
{
    auto newWindow = std::make_unique<Beyond::Window>();

    // Lempar parameternya ke Initialize
    if (!newWindow->Initialize(title, width, height, isTransparent))
    {
        return nullptr;
    }

    newWindow->SetTickCallback([]() {
        // Gunakan angka statis 0.016f (1/60 detik) BUKAN variabel elapsedTime
        Framework::Instance()->Update(0.016f);
        Framework::Instance()->Render(0.016f);
        });

    Beyond::Window* ptr = newWindow.get();
    windows.push_back(std::move(newWindow));

    // Trigger agar urutan window diperbarui
    MarkPriorityDirty();

    return ptr;
}

void WindowManager::DestroyWindow(Beyond::Window* targetWindow)
{
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [targetWindow](const std::unique_ptr<Beyond::Window>& p) {
                return p.get() == targetWindow;
            }),
        windows.end());

    // Trigger re-sort
    MarkPriorityDirty();
}

void WindowManager::ClearAll()
{
    windows.clear();
}