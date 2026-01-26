#include "WindowShatter.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>
#include <windows.h> 

// =========================================================
// WINDOW SHATTER IMPLEMENTATION
// =========================================================

WindowShatter::WindowShatter(const char* title, DirectX::XMFLOAT2 startPos, DirectX::XMFLOAT2 velocity, int size, int priority)
    : m_width(static_cast<float>(size))
    , m_height(static_cast<float>(size))
{
    // Cache screen metrics once
    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Initialize as Virtual (Fake) Object
    m_isNativeWindow = false;
    m_window = nullptr;
    m_virtualWorldPos = { startPos.x, 0.0f, startPos.y }; // Map 2D -> 3D Plane

    // Physics Setup
    m_physics.velocity = velocity;
    m_physics.deceleration = 0.95f;
}

WindowShatter::~WindowShatter()
{
    if (m_window)
    {
        WindowManager::Instance().DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void WindowShatter::Update(float dt)
{
    if (m_markedForDestroy) return;

    if (!m_isNativeWindow)
    {
        UpdateVirtualState(dt);
    }
    else
    {
        UpdateNativeState(dt);
    }
}

void WindowShatter::UpdateVirtualState(float dt)
{
    // Move in virtual 3D space
    m_virtualWorldPos.x += m_physics.velocity.x * dt;
    m_virtualWorldPos.z += m_physics.velocity.y * dt;

    // Check bounds to trigger native transition
    float screenX, screenY;
    ConvertWorldToScreen(m_virtualWorldPos, screenX, screenY);

    if (screenX <= 0 || screenX >= m_screenWidth || screenY <= 0 || screenY >= m_screenHeight)
    {
        TransitionToNativeWindow();
    }
}

void WindowShatter::UpdateNativeState(float dt)
{
    if (!m_window) return;

    // 1. Physics Deceleration
    float decel = (m_physics.bounceCount > m_physics.maxBounces) ? 0.9f : m_physics.deceleration;
    m_physics.velocity.x *= decel;
    m_physics.velocity.y *= decel;

    // 2. Move Window
    int curX, curY;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &curX, &curY);

    float moveX = m_physics.velocity.x * dt;
    float moveY = m_physics.velocity.y * dt;

    // Optimization: Only call SDL API if movement is significant (> 1 pixel)
    if (std::abs(moveX) >= 1.0f || std::abs(moveY) >= 1.0f)
    {
        SDL_SetWindowPosition(m_window->GetSDLWindow(),
            curX + static_cast<int>(moveX),
            curY + static_cast<int>(moveY));
    }

    // 3. Shrink Size
    float shrink = m_shrinkRate * dt;
    m_width = max(MIN_SIZE, m_width - shrink);
    m_height = max(MIN_SIZE, m_height - shrink);

    // Keep window centered while shrinking
    m_window->Resize(static_cast<int>(m_width), static_cast<int>(m_height));

    EnforceScreenBounds();
}

void WindowShatter::EnforceScreenBounds()
{
    if (!m_window) return;

    int x, y;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &x, &y);

    bool bounced = false;

    // Horizontal Check
    if (x <= 0)
    {
        x = 0;
        m_physics.velocity.x *= -m_physics.bounceDamping;
        bounced = true;
    }
    else if (x + (int)m_width >= m_screenWidth)
    {
        x = m_screenWidth - (int)m_width;
        m_physics.velocity.x *= -m_physics.bounceDamping;
        bounced = true;
    }

    // Vertical Check
    if (y <= 0)
    {
        y = 0;
        m_physics.velocity.y *= -m_physics.bounceDamping;
        bounced = true;
    }
    else if (y + (int)m_height >= m_screenHeight)
    {
        y = m_screenHeight - (int)m_height;
        m_physics.velocity.y *= -m_physics.bounceDamping;
        bounced = true;
    }

    if (bounced)
    {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), x, y);
        m_physics.bounceCount++;

        // Add slight randomness to bounce
        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> jitter(-50.0f, 50.0f);
        m_physics.velocity.x += jitter(rng);
    }
}

void WindowShatter::TransitionToNativeWindow()
{
    float screenX, screenY;
    ConvertWorldToScreen(m_virtualWorldPos, screenX, screenY);

    // Create the actual OS window
    m_window = WindowManager::Instance().CreateGameWindow("Shatter", (int)m_width, (int)m_height);

    if (m_window)
    {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), (int)screenX, (int)screenY);

        // Setup dedicated camera
        m_camera = std::make_shared<Camera>();
        m_camera->SetRotation(90.0f, 0.0f, 0.0f); // Top-down view
        m_window->SetCamera(m_camera.get());

        m_isNativeWindow = true;
    }
}

void WindowShatter::ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outX, float& outY) const
{
    outX = (m_screenWidth * 0.5f) + (worldPos.x * PIXEL_TO_UNIT_RATIO);
    outY = (m_screenHeight * 0.5f) - (worldPos.z * PIXEL_TO_UNIT_RATIO);
}

// =========================================================
// MANAGER IMPLEMENTATION
// =========================================================

void WindowShatterManager::TriggerExplosion(DirectX::XMFLOAT2 centerWorldPos, int count)
{
    // Start the staggered spawning sequence
    m_pendingSpawnPos = centerWorldPos;
    m_pendingSpawnCount = count;
    m_spawnTimer = 0.0f; // Force immediate first spawn
}

void WindowShatterManager::Update(float dt)
{
    // 1. Handle Staggered Spawning (Optimization)
    if (m_pendingSpawnCount > 0)
    {
        m_spawnTimer += dt;
        if (m_spawnTimer >= SPAWN_INTERVAL)
        {
            SpawnSingleInstance(m_pendingSpawnPos, m_pendingSpawnCount);
            m_pendingSpawnCount--;
            m_spawnTimer = 0.0f; // Reset timer
        }
    }

    // 2. Update all active instances
    for (auto& shatter : m_shatters)
    {
        shatter->Update(dt);
    }

    // 3. Remove dead instances
    m_shatters.erase(std::remove_if(m_shatters.begin(), m_shatters.end(),
        [](const std::unique_ptr<WindowShatter>& s) { return s->ShouldDestroy(); }),
        m_shatters.end());
}

void WindowShatterManager::SpawnSingleInstance(DirectX::XMFLOAT2 centerPos, int index)
{
    static std::mt19937 gen(std::random_device{}());

    // Calculate angle with jitter
    float totalShatters = 8.0f;
    float angleStep = 360.0f / totalShatters;
    std::uniform_real_distribution<float> jitter(-15.0f, 15.0f);
    float angleRad = DirectX::XMConvertToRadians((index * angleStep) + jitter(gen));

    // Randomize properties
    std::uniform_real_distribution<float> speedDist(300.0f, 700.0f);
    std::uniform_real_distribution<float> sizeDist(200.0f, 300.0f);
    std::uniform_real_distribution<float> offsetDist(-10.0f, 10.0f);

    float speed = speedDist(gen);
    DirectX::XMFLOAT2 velocity = { cosf(angleRad) * speed, sinf(angleRad) * speed };
    DirectX::XMFLOAT2 spawnPos = { centerPos.x + offsetDist(gen), centerPos.y + offsetDist(gen) };

    int size = static_cast<int>(sizeDist(gen));
    int priority = (index % 2 == 0) ? -1 : 999;

    char title[32];
    sprintf_s(title, "Shatter_%d", index);

    m_shatters.push_back(std::make_unique<WindowShatter>(title, spawnPos, velocity, size, priority));
}

void WindowShatterManager::Clear()
{
    m_shatters.clear();
    m_pendingSpawnCount = 0;
}