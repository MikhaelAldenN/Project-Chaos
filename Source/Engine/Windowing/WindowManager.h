#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <windows.h>
#include <mutex>
#include "GameWindow.h"

// Forward Declaration
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

    // --- CORE FUNCTIONS ---
    void Update(float dt);
    void RenderAll(float dt, Scene* scene);
    void HandleResize(HWND hWnd, int width, int height);
    void ClearAll();

    // --- USER FUNCTIONS ---
    GameWindow* CreateGameWindow(const char* title, int width, int height);
    void DestroyWindow(GameWindow* targetWindow);
    void EnforceWindowPriorities();
    void MarkPriorityDirty() { m_dirtyPriority = true; }

    void SetDebugWindow(GameWindow* win) { debugWindow = win; }
    GameWindow* GetDebugWindow() const { return debugWindow; }

    // --------------------------------------------------------
    // [BARU] Tambahkan Helper Functions ini:
    // --------------------------------------------------------

    // 1. Cek apakah ada window yang hidup (Dipakai di Main.cpp)
    bool HasWindows() const { return !windows.empty(); }

    // 2. Ambil window berdasarkan index (Dipakai di Framework.cpp untuk ambil Main Window)
    GameWindow* GetWindowByIndex(size_t index)
    {
        if (index < windows.size()) return windows[index].get();
        return nullptr;
    }

    void SetTopmost(bool enabled) { m_topmostEnabled = enabled; MarkPriorityDirty(); }
    bool IsTopmost() const { return m_topmostEnabled; }

private:
    WindowManager() = default;
    ~WindowManager() = default;
    WindowManager(const WindowManager&) = delete;
    void operator=(const WindowManager&) = delete;

    bool m_topmostEnabled = false;
private:
    std::vector<std::unique_ptr<GameWindow>> windows;

    GameWindow* debugWindow = nullptr;

    bool m_dirtyPriority = false;
    mutable std::mutex m_windowsMutex;
};