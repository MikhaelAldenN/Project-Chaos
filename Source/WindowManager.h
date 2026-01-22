#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <windows.h>
#include "GameWindow.h"

// Forward Declaration biar tidak berat compile
class Scene;

class WindowManager
{
public:
    // --- SINGLETON PATTERN ---
    static WindowManager& Instance()
    {
        static WindowManager instance;
        return instance;
    }

    // --- CORE FUNCTIONS (Dipanggil Framework) ---
    void Update(float dt);
    void RenderAll(float dt, Scene* scene);
    void HandleResize(HWND hWnd, int width, int height);
    void ClearAll(); // Hapus semua window anak (misal saat Reset/Shutdown)

    // --- USER FUNCTIONS (Dipanggil SceneGame) ---
    // Membuat window baru dan mengembalikannya agar bisa disetting (Camera, Posisi, dll)
    GameWindow* CreateGameWindow(const char* title, int width, int height);

    // Menghancurkan window tertentu
    void DestroyWindow(GameWindow* targetWindow);

    void EnforceWindowPriorities();

private:
    WindowManager() = default;
    ~WindowManager() = default;
    // Mencegah copy/assignment
    WindowManager(const WindowManager&) = delete;
    void operator=(const WindowManager&) = delete;

private:
    // Gudang penyimpanan window anak (Dynamic Windows)
    std::vector<std::unique_ptr<GameWindow>> windows;
};