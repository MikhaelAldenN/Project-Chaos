#pragma once
#include <d3d11.h>

// Forward declarations agar kita tidak perlu #include berat-berat di header
class NaviBoss;
class Camera;

class INaviPhase {
public:
    virtual ~INaviPhase() = default;

    // Dipanggil SATU KALI saat fase ini baru saja dimulai
    virtual void Enter(NaviBoss* boss) = 0;

    // Dipanggil setiap frame (60 FPS)
    virtual void Update(float dt, NaviBoss* boss) = 0;

    // Dipanggil setiap frame untuk menggambar 3D/Sprite
    virtual void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) = 0;

    // Dipanggil SATU KALI saat boss pindah ke fase lain atau mati
    virtual void Exit(NaviBoss* boss) = 0;
};