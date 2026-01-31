#include <windows.h>
#include <memory>
#include <SDL3/SDL.h> 
#include <iostream> // Untuk print error
#include <exception> // Untuk menangkap error
#include "WindowManager.h"
#include <thread>

#include "Framework.h"

void EmergencyWatchdog()
{
    while (true)
    {
        // Cek Kombinasi Tombol: CTRL + F12
        // Kita pakai GetAsyncKeyState karena dia cek hardware langsung, 
        // tidak peduli window game lagi hang atau tidak.
        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        bool f12Pressed = (GetAsyncKeyState(VK_F12) & 0x8000);

        if (ctrlPressed && f12Pressed)
        {
            // Opsi 1: Bunyi Beep biar tau tombol bekerja
            Beep(200, 50);

            // Opsi 2: Print Debug
            OutputDebugStringA("!!! EMERGENCY EXIT TRIGGERED !!!\n");

            // Opsi 3: NUCLEAR OPTION (Kill Process Detik Itu Juga)
            // ExitProcess tidak menunggu destructor atau cleanup. Langsung mati.
            ExitProcess(-1);
        }

        // Tidur sebentar biar gak makan CPU (cek 10x per detik)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char* argv[])
{
    std::thread safetyThread(EmergencyWatchdog);
    safetyThread.detach();

    AllocConsole();

    // 1. Setup Console untuk melihat error log (Optional tapi berguna)
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    // 2. Init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        MessageBoxA(NULL, SDL_GetError(), "SDL Init Failed", MB_OK | MB_ICONERROR);
        return -1;
    }

    try
    {
        // =========================================================
        // PROGRAM UTAMA DI DALAM BLOK TRY
        // =========================================================

        // Buat Framework (Ini akan memicu semua constructor Scene, Player, dll)
        auto framework = std::make_unique<Framework>();

        bool running = true;
        Uint64 lastTime = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) running = false;
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
            }

            Uint64 currentTime = SDL_GetPerformanceCounter();
            float elapsedTime = (float)(currentTime - lastTime) / (float)frequency;
            lastTime = currentTime;

            if (elapsedTime > 0.05f) elapsedTime = 0.05f;

            if (framework)
            {
                framework->Update(elapsedTime);
                framework->Render(elapsedTime);
            }

            if (!WindowManager::Instance().HasWindows())
            {
                running = false;
            }
        }
        // =========================================================
    }
    catch (const std::exception& e)
    {
        // TANGKAP ERRORNYA DAN TAMPILKAN DI LAYAR!
        std::string errorMessage = "Runtime Error Occurred:\n";
        errorMessage += e.what();

        // Tampilkan Popup Error agar kita tahu apa salahnya
        MessageBoxA(NULL, errorMessage.c_str(), "CRITICAL ERROR", MB_OK | MB_ICONERROR);

        // Print ke console juga
        std::cerr << "!!! CRITICAL ERROR: " << e.what() << std::endl;

        SDL_Quit();
        return -1;
    }
    catch (...)
    {
        MessageBoxA(NULL, "Unknown Error Occurred!", "CRITICAL ERROR", MB_OK | MB_ICONERROR);
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}