#pragma once
#include <windows.h>
#include <memory>
#include "System/HighResolutionTimer.h"
#include "GameWindow.h"
#include "Scene.h"

class Framework
{
public:
    Framework();
    ~Framework();
    static Framework* Instance();

    void Update(float elapsedTime);
    void Render(float elapsedTime);
    void ForceUpdateRender();
    void ChangeScene(std::unique_ptr<Scene> newScene);
    void Quit();

    // Helper untuk mengambil Main Window (Window index 0)
    GameWindow* GetMainWindow() const;

    // [PERBAIKAN 1] Tambahkan deklarasi HandleMessage agar bisa diakses Main.cpp
    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // [PERBAIKAN 2] Tambahkan deklarasi CalculateFrameStats
    void CalculateFrameStats(float dt);

    static Framework* pInstance;
    HighResolutionTimer timer;

    // Variabel mainWindow SUDAH DIHAPUS, jadi jangan tambahkan lagi.
    // Kita akan gunakan GetMainWindow() di cpp.

    std::unique_ptr<Scene> scene;
    std::unique_ptr<Scene> nextScene;
};