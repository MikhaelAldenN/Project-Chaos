#include "InputHelper.h"
#include "Engine/Common/Constants.h"

namespace Beyond {
    DirectX::XMFLOAT3 InputHelper::GetMouseWorldPos(const DirectX::XMFLOAT3& cameraWorldPos) {
        // [SDL3] Ambil posisi mouse global (seluruh monitor)
        float mouseX, mouseY;
        SDL_GetGlobalMouseState(&mouseX, &mouseY);

        // [SDL3] Ambil resolusi monitor utama (otomatis handle DPI Scaling)
        SDL_Rect displayBounds;
        SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &displayBounds);

        float monitorCenterX = displayBounds.w / 2.0f;
        float monitorCenterY = displayBounds.h / 2.0f;

        // Hitung jarak mouse dari tengah monitor (dalam pixel)
        float pixelOffsetX = mouseX - monitorCenterX;
        float pixelOffsetY = mouseY - monitorCenterY;

        // Konversi ke World Unit + Tambahkan posisi kamera (untuk SceneGame yang bergerak)
        float worldX = (pixelOffsetX / Config::PIXEL_TO_UNIT_RATIO) + cameraWorldPos.x;
        float worldZ = (-pixelOffsetY / Config::PIXEL_TO_UNIT_RATIO) + cameraWorldPos.z;

        return DirectX::XMFLOAT3(worldX, 0.0f, worldZ);
    }
}