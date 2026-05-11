#include "NaviPhaseNormal.h"
#include "NaviPhaseWindowkill.h"
#include "NaviBoss.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>

void NaviPhaseNormal::Enter(NaviBoss* boss) {
    if (!boss) return;

    // 1. Ambil resolusi monitor murni
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 2. Terapkan Borderless Fullscreen ke Main Window (Index 0)
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_Window* sdlWin = mainWindow->GetSDLWindow();

        // Hapus bingkai OS (Title bar, tombol silang, dll)
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowResizable(sdlWin, false);

        // Posisikan ke pojok kiri atas monitor dan lebarkan!
        SDL_SetWindowPosition(sdlWin, 0, 0);
        SDL_SetWindowSize(sdlWin, screenW, screenH);
    }
}

void NaviPhaseNormal::Update(float dt, NaviBoss* boss) {

}

void NaviPhaseNormal::Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) {
    // Render peluru atau efek khusus Fase 1 di sini.
    // Catatan: Kepala Navi sudah otomatis digambar oleh NaviBoss (Context),
    // jadi Anda tidak perlu memanggil m_faceSprite->Render() lagi di sini.
}

void NaviPhaseNormal::Exit(NaviBoss* boss) {
    // Bersihkan semua peluru Fase 1 atau efek partikel jika ada.
}