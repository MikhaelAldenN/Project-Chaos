#pragma once

#include <DirectXMath.h>
#include "GameWindow.h"
#include <memory>
#include <vector>    // WAJIB: Untuk std::vector
#include <string>    // Untuk sprintf atau string jika perlu

// Struct untuk menyimpan state fisika window shatter
struct ShatterPhysics
{
    DirectX::XMFLOAT2 velocity;
    float angularVelocity;
    float deceleration;
    float gravity;
    float bounceDamping;
    int bounceCount;
    int maxBounces;
};

class WindowShatter
{
public:
    WindowShatter(
        const char* title,
        DirectX::XMFLOAT2 startPos,
        DirectX::XMFLOAT2 initialVelocity,
        int initialWidth,
        int initialHeight,
        float deceleration,
        float shrinkRate
    );

    ~WindowShatter();

    void Update(float dt);
    bool ShouldDestroy() const { return markedForDestroy; }
    GameWindow* GetWindow() const { return window; }

private:
    void UpdatePhysics(float dt);
    void UpdateSize(float dt);
    void CheckBounds();
    void ApplyBounce(bool horizontal);

private:
    GameWindow* window;
    ShatterPhysics physics;

    float shrinkRate;
    float currentWidth;
    float currentHeight;

    static constexpr float MIN_SIZE = 150.0f;
    bool markedForDestroy;

    int screenWidth;
    int screenHeight;
};

// ============================================================
// MANAGER CLASS
// ============================================================
class WindowShatterManager
{
public:
    static WindowShatterManager& Instance()
    {
        static WindowShatterManager instance;
        return instance;
    }

    void SpawnShatterExplosion(
        DirectX::XMFLOAT2 centerPos,
        int count = 10,
        float minSpeed = 300.0f,
        float maxSpeed = 800.0f
    );

    void Update(float dt);
    void Clear();
    bool HasActiveShatter() const { return !shatters.empty(); }
    int GetActiveCount() const { return static_cast<int>(shatters.size()); }

private:
    WindowShatterManager() = default;
    ~WindowShatterManager() { Clear(); }
    WindowShatterManager(const WindowShatterManager&) = delete;
    void operator=(const WindowShatterManager&) = delete;

    // Baris ini sering error jika <vector> tidak di-include
    std::vector<std::unique_ptr<WindowShatter>> shatters;
};
// JANGAN ADA KURUNG KURAWAL TUTUP DI SINI JIKA TIDAK ADA NAMESPACE