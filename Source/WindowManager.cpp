#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>

void WindowManager::Update(float dt)
{
    // ... logika update window ...
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

    // -------------------------------------------------------------------------
    // PERUBAHAN 1: SORT ASCENDING (0, 1, 2...)
    // Kita urutkan dari yang PALING PENTING (0) ke yang kurang penting.
    // Urutan: [LENS (0), TRACKING (1)]
    // -------------------------------------------------------------------------
    std::sort(sortedWindows.begin(), sortedWindows.end(),
        [](GameWindow* a, GameWindow* b) {
            return a->GetPriority() < b->GetPriority(); // <--- Ganti jadi Smaller (<) First
        });

    // -------------------------------------------------------------------------
    // PERUBAHAN 2: RELATIVE CHAINING
    // Kita tumpuk window seperti kue lapis dari atas ke bawah.
    // -------------------------------------------------------------------------

    // Awalnya, "Atap" kita adalah posisi paling atas absolut (HWND_TOPMOST)
    HWND hInsertAfter = HWND_TOPMOST;

    for (GameWindow* win : sortedWindows)
    {
        // Posisikan window ini TEPAT DI BAWAH 'hInsertAfter'
        // Loop 1 (Lens): Ditaruh di bawah HWND_TOPMOST (Jadi paling atas)
        // Loop 2 (Tracking): Ditaruh di bawah LENS (Jadi nomor 2)

        SetWindowPos(win->GetHWND(),
            hInsertAfter,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        // Window ini sekarang menjadi "Atap" untuk window berikutnya
        hInsertAfter = win->GetHWND();
    }
}

void WindowManager::RenderAll(float dt, Scene* scene)
{
    if (!scene) return;

    // Loop semua window di gudang
    for (auto& win : windows)
    {
        // 1. Siapkan Layar Window Ini
        //    Kita kasih warna background agak merah biar beda sama Main Window
        win->BeginRender(0.5f, 0.5f, 0.5f);

        // 2. Resize Aspect Ratio Scene
        //    PENTING: Agar gambar tidak gepeng di window kecil
        scene->OnResize(win->GetWidth(), win->GetHeight());

        // 3. Render Scene
        //    Menggunakan Kamera yang dipegang oleh Window tersebut
        scene->Render(dt, win->GetCamera());

        // 4. Tampilkan
        win->EndRender();
    }
}

void WindowManager::HandleResize(HWND hWnd, int width, int height)
{
    // Cari window mana yang punya HWND ini
    for (auto& win : windows)
    {
        if (win->GetHWND() == hWnd)
        {
            win->Resize(width, height);
            return; // Ketemu, selesai
        }
    }
}

GameWindow* WindowManager::CreateGameWindow(const char* title, int width, int height)
{
    // 1. Buat Window Baru
    auto newWindow = std::make_unique<GameWindow>(title, width, height);

    // 2. Ambil Raw Pointer untuk dikembalikan ke peminta (SceneGame)
    GameWindow* ptr = newWindow.get();

    // 3. Simpan ke vector (ownership pindah ke Manager)
    windows.push_back(std::move(newWindow));

    return ptr;
}

void WindowManager::DestroyWindow(GameWindow* targetWindow)
{
    // Hapus window dari vector (Otomatis men-trigger destructor GameWindow -> Close Window)
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