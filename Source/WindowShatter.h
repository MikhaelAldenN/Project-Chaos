#pragma once

#include <memory>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "GameWindow.h"
#include "Camera.h"
#include "System/ModelRenderer.h"

// Physics state for shatter instances
struct ShatterPhysics
{
    DirectX::XMFLOAT2 velocity{ 0.0f, 0.0f };
    float angularVelocity = 0.0f;
    float deceleration = 0.0f;
    float gravity = 0.0f;
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
        DirectX::XMFLOAT2 initialVelocity,
        int initialWidth,
        int initialHeight,
        float deceleration,
        float shrinkRate,
        int priority
    );

    ~WindowShatter();

    void Update(float dt);

    void RenderFake(ModelRenderer* renderer);

    // --- TAMBAHKAN 3 FUNGSI INI ---
    [[nodiscard]] bool IsNativeWindow() const { return m_isNativeWindow; }
    [[nodiscard]] DirectX::XMFLOAT3 GetFakeWorldPos() const { return m_fakeWorldPos; }
    [[nodiscard]] DirectX::XMFLOAT2 GetCurrentSize() const { return { m_currentWidth, m_currentHeight }; }    // ------------------------------

    // Getters
    [[nodiscard]] bool ShouldDestroy() const { return m_markedForDestroy; }
    [[nodiscard]] GameWindow* GetWindow() const { return m_window; }
    [[nodiscard]] Camera* GetCamera() const { return m_camera.get(); }

private:
    void UpdatePhysics(float dt);
    void UpdateSize(float dt);
    void CheckBounds();
    void ApplyBounce(bool horizontal);
    void TransitionToNativeWindow();

    // --- TAMBAHKAN 2 BARIS INI ---
    static constexpr float PIXEL_TO_UNIT_RATIO = 40.0f; // Sesuai dengan yang ada di SceneGameBeyond
    void ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY) const;

private:
    GameWindow* m_window = nullptr;
    std::shared_ptr<Camera> m_camera;
    ShatterPhysics m_physics;

    float m_shrinkRate = 0.0f;
    float m_currentWidth = 0.0f;
    float m_currentHeight = 0.0f;

    static constexpr float MIN_SIZE = 150.0f;
    static constexpr float STOP_THRESHOLD_SQ = 100.0f; // 10.0f * 10.0f

    bool m_markedForDestroy = false;
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    bool m_isNativeWindow = false;
    DirectX::XMFLOAT3 m_fakeWorldPos; // Posisi 3D di dalam engine

};

// ============================================================
// MANAGER CLASS (Singleton)
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
        float maxSpeed = 800.0f);

    void Update(float dt);
    void Clear();

    [[nodiscard]] bool HasActiveShatter() const { return !m_shatters.empty(); }
    [[nodiscard]] int GetActiveCount() const { return static_cast<int>(m_shatters.size()); }
    [[nodiscard]] const std::vector<std::unique_ptr<WindowShatter>>& GetShatters() const { return m_shatters; }

    void SpawnSingleShatter(DirectX::XMFLOAT2 centerPos, int index);

private:
    WindowShatterManager() = default;
    ~WindowShatterManager() { Clear(); }
    WindowShatterManager(const WindowShatterManager&) = delete;
    void operator=(const WindowShatterManager&) = delete;

private:
    std::vector<std::unique_ptr<WindowShatter>> m_shatters;
};