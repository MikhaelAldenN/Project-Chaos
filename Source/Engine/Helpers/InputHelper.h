#pragma once
#include <DirectXMath.h>
#include <SDL3/SDL.h>

namespace Beyond {
    class InputHelper {
    public:
        // Fungsi universal untuk mengubah koordinat Mouse OS ke Koordinat Dunia Game
        static DirectX::XMFLOAT3 GetMouseWorldPos(const DirectX::XMFLOAT3& cameraWorldPos);
    };
}