#pragma once

#include <windows.h>
#include <memory>
#include <vector>

#include "System/HighResolutionTimer.h"
#include "GameWindow.h"
#include "Scene.h"
#include "BitmapFont.h" 
#include "ResourceManager.h"

// Core application class managing the main loop, window, and scene transitions.
// Implements the Singleton pattern.
class Framework
{
public:
    Framework();
    ~Framework();

    static Framework* Instance();

    // --- Main Loop ---
    void Update(float elapsedTime);
    void Render(float elapsedTime);

    // Manually steps the loop. Used during blocking window events (e.g., resizing/dragging).
    void ForceUpdateRender();

    // Queues a scene switch for the next update cycle.
    void ChangeScene(std::unique_ptr<Scene> newScene);

    // --- Window & Message Handling ---
    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    GameWindow* GetMainWindow() const { return mainWindow.get(); }

private:
    void CalculateFrameStats(float dt);

    static Framework* pInstance;
    HighResolutionTimer timer;
    std::unique_ptr<GameWindow> mainWindow;
    std::unique_ptr<Scene> scene;
    std::unique_ptr<Scene> nextScene;
};