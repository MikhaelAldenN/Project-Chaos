#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "GameWindow.h"
#include "Camera.h"

// Physics parameters for the shatter effect
struct ShatterPhysics
{
    DirectX::XMFLOAT2 velocity{ 0.0f, 0.0f };
    float deceleration = 0.0f;
    float bounceDamping = 0.7f;
    int bounceCount = 0;
    int maxBounces = 3;
};

class WindowShatter
{
public:
    WindowShatter(
        const char* title,
        DirectX::XMFLOAT2 startPos,
        DirectX::XMFLOAT2 velocity,
        int size,
        int priority
    );
    ~WindowShatter();

    void Update(float dt);

    // --- State Getters ---
    [[nodiscard]] bool IsNativeWindow() const { return m_isNativeWindow; }
    [[nodiscard]] bool ShouldDestroy() const { return m_markedForDestroy; }

    // --- Data Getters ---
    [[nodiscard]] DirectX::XMFLOAT3 GetVirtualWorldPos() const { return m_virtualWorldPos; }
    [[nodiscard]] DirectX::XMFLOAT2 GetSize() const { return { m_width, m_height }; }
    [[nodiscard]] GameWindow* GetWindow() const { return m_window; }
    [[nodiscard]] Camera* GetCamera() const { return m_camera.get(); }

private:
    void UpdateVirtualState(float dt);
    void UpdateNativeState(float dt);
    void TransitionToNativeWindow();

    // Helper to keep window inside screen bounds
    void EnforceScreenBounds();
    void ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outX, float& outY) const;

private:
    // Core Components
    GameWindow* m_window = nullptr;
    std::shared_ptr<Camera> m_camera;
    ShatterPhysics m_physics;

    // State
    bool m_isNativeWindow = false;
    bool m_markedForDestroy = false;
    DirectX::XMFLOAT3 m_virtualWorldPos; // Position in 3D world before becoming a window

    // Dimensions
    float m_width = 0.0f;
    float m_height = 0.0f;
    float m_shrinkRate = 50.0f;
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Constants
    static constexpr float MIN_SIZE = 150.0f;
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;
};

// ============================================================
// MANAGER CLASS (Singleton)
// Handles the lifecycle and optimization (staggered spawning)
// ============================================================
class WindowShatterManager
{
public:
    static WindowShatterManager& Instance()
    {
        static WindowShatterManager instance;
        return instance;
    }

    // Queues an explosion effect. The manager handles the staggered spawning internally.
    void TriggerExplosion(DirectX::XMFLOAT2 centerWorldPos, int count = 8);

    void Update(float dt);
    void Clear();

    [[nodiscard]] const std::vector<std::unique_ptr<WindowShatter>>& GetShatters() const { return m_shatters; }
    [[nodiscard]] int GetActiveCount() const { return static_cast<int>(m_shatters.size()); }

private:
    WindowShatterManager() = default;
    ~WindowShatterManager() { Clear(); }

    // Internal helper to spawn a single instance
    void SpawnSingleInstance(DirectX::XMFLOAT2 centerPos, int index);

private:
    std::vector<std::unique_ptr<WindowShatter>> m_shatters;

    // Staggered Spawning State
    DirectX::XMFLOAT2 m_pendingSpawnPos = { 0,0 };
    int m_pendingSpawnCount = 0;
    float m_spawnTimer = 0.0f;
    static constexpr float SPAWN_INTERVAL = 0.05f; // 50ms delay between windows to prevent freeze
};