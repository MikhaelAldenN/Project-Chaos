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

    // DrawGUI HANYA SEKALI  sudah benar, tapi OnResize jangan dipanggil tiap window!
    scene->DrawGUI();

    bool vsyncApplied = false;
    auto context = Graphics::Instance().GetDeviceContext();
    auto mainWindow = Framework::Instance()->GetMainWindow();

    for (auto& win : windows)
    {
        if (!win->IsVisible()) continue;
        if (win.get() != mainWindow && !win->ShouldRender(dt)) continue;

        // HAPUS: scene->OnResize(win->GetWidth(), win->GetHeight());
        // Ganti dengan: hanya panggil jika ukuran window berubah dari frame sebelumnya
        // Simpan ukuran terakhir di Beyond::Window itu sendiri (lihat Fix 3)
        int w = win->GetWidth(), h = win->GetHeight();
        if (w != win->m_lastRenderedW || h != win->m_lastRenderedH)
        {
            scene->OnResize(w, h);
            win->m_lastRenderedW = w;
            win->m_lastRenderedH = h;
        }

        float clearAlpha = win->IsTransparent() ? win->GetBackgroundAlpha() : 1.0f;
        if (win->IsTransparent())
            win->BeginRender(0.0f, 0.0f, 0.0f, clearAlpha);
        else
            win->BeginRender(0.02f, 0.04f, 0.15f, clearAlpha);

        scene->Render(dt, win->GetCamera());

        if (win.get() == mainWindow) ImGuiRenderer::Render(context);

        int syncInterval = (!vsyncApplied) ? 1 : 0;
        if (!vsyncApplied) vsyncApplied = true;
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