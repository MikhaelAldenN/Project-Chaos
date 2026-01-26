#include "WindowShatter.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>
#include <windows.h> 

WindowShatter::WindowShatter(const char* title, DirectX::XMFLOAT2 startPos, DirectX::XMFLOAT2 velocity, int size, int priority)
    : m_width(static_cast<float>(size))
    , m_height(static_cast<float>(size))
    , m_title(title) // [FIX] Simpan title
{
    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    m_isNativeWindow = false;
    m_window = nullptr;
    m_virtualWorldPos = { startPos.x, 0.0f, startPos.y };

    m_physics.velocity = velocity;
    m_physics.deceleration = 0.99f;
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
    m_timeAlive += dt;

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
    float worldSpeedX = m_physics.velocity.x / PIXEL_TO_UNIT_RATIO;
    float worldSpeedZ = m_physics.velocity.y / PIXEL_TO_UNIT_RATIO;

    m_virtualWorldPos.x += worldSpeedX * dt;
    m_virtualWorldPos.z += worldSpeedZ * dt;

    if (m_timeAlive > 0.05f)
    {
        TransitionToNativeWindow();
    }
}

void WindowShatter::UpdateNativeState(float dt)
{
    if (!m_window) return;

    m_physics.velocity.x *= m_physics.deceleration;
    m_physics.velocity.y *= m_physics.deceleration;

    int curX, curY;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &curX, &curY);

    float moveX = m_physics.velocity.x * dt;
    float moveY = m_physics.velocity.y * dt;

    int nextX = curX + static_cast<int>(moveX);
    int nextY = curY + static_cast<int>(moveY);

    SDL_SetWindowPosition(m_window->GetSDLWindow(), nextX, nextY);

    float shrink = m_shrinkRate * dt;
    m_width = max(10.0f, m_width - shrink);
    m_height = max(10.0f, m_height - shrink);

    if (m_width > 20 && m_height > 20) {
        m_window->Resize(static_cast<int>(m_width), static_cast<int>(m_height));
    }
    else {
        m_markedForDestroy = true;
    }

    EnforceScreenBounds();
}

void WindowShatter::TransitionToNativeWindow()
{
    float screenX, screenY;
    ConvertWorldToScreen(m_virtualWorldPos, screenX, screenY);

    // [FIX] Gunakan m_title yang unik agar WindowManager tidak menolak duplikat
    m_window = WindowManager::Instance().CreateGameWindow(m_title.c_str(), (int)m_width, (int)m_height);

    if (m_window)
    {
        int finalX = (int)(screenX - (m_width * 0.5f));
        int finalY = (int)(screenY - (m_height * 0.5f));

        SDL_SetWindowPosition(m_window->GetSDLWindow(), finalX, finalY);

        m_camera = std::make_shared<Camera>();
        m_camera->SetRotation(90.0f, 0.0f, 0.0f);
        m_window->SetCamera(m_camera.get());

        m_isNativeWindow = true;
    }
    else
    {
        // Debugging: Jika window gagal dibuat (misal null), hancurkan objek agar tidak menumpuk di memori
        m_markedForDestroy = true;
    }
}

void WindowShatter::EnforceScreenBounds()
{
    if (!m_window) return;
    int x, y;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &x, &y);
    bool bounced = false;

    if (x <= 0) { x = 0; m_physics.velocity.x *= -m_physics.bounceDamping; bounced = true; }
    else if (x + m_width >= m_screenWidth) { x = m_screenWidth - (int)m_width; m_physics.velocity.x *= -m_physics.bounceDamping; bounced = true; }

    if (y <= 0) { y = 0; m_physics.velocity.y *= -m_physics.bounceDamping; bounced = true; }
    else if (y + m_height >= m_screenHeight) { y = m_screenHeight - (int)m_height; m_physics.velocity.y *= -m_physics.bounceDamping; bounced = true; }

    if (bounced) {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), x, y);
        m_physics.bounceCount++;
    }
}

void WindowShatter::ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outX, float& outY) const
{
    outX = (m_screenWidth * 0.5f) + (worldPos.x * PIXEL_TO_UNIT_RATIO);
    outY = (m_screenHeight * 0.5f) - (worldPos.z * PIXEL_TO_UNIT_RATIO);
}

// Manager Implementation
void WindowShatterManager::TriggerExplosion(DirectX::XMFLOAT2 centerWorldPos, int count)
{
    for (int i = 0; i < count; ++i)
    {
        SpawnSingleInstance(centerWorldPos, i, count);
    }
}

void WindowShatterManager::Update(float dt)
{
    for (auto& shatter : m_shatters)
    {
        shatter->Update(dt);
    }

    m_shatters.erase(std::remove_if(m_shatters.begin(), m_shatters.end(),
        [](const std::unique_ptr<WindowShatter>& s) { return s->ShouldDestroy(); }),
        m_shatters.end());
}

void WindowShatterManager::SpawnSingleInstance(DirectX::XMFLOAT2 centerPos, int index, int totalCount)
{
    static std::mt19937 gen(std::random_device{}());

    float angleStep = 360.0f / totalCount;
    std::uniform_real_distribution<float> jitterAngle(-5.0f, 5.0f);
    float angleRad = DirectX::XMConvertToRadians((index * angleStep) + jitterAngle(gen));

    std::uniform_real_distribution<float> speedDist(800.0f, 1500.0f);
    float speed = speedDist(gen);

    DirectX::XMFLOAT2 velocity = { cosf(angleRad) * speed, sinf(angleRad) * speed };

    std::uniform_real_distribution<float> offsetDist(-2.0f, 2.0f);
    DirectX::XMFLOAT2 spawnPos = { centerPos.x + offsetDist(gen), centerPos.y + offsetDist(gen) };

    int size = static_cast<int>(std::uniform_real_distribution<float>(250.0f, 350.0f)(gen));

    // [PENTING] Gunakan nama unik agar WindowManager bisa membedakan
    char title[64];
    sprintf_s(title, "Shatter_%u_%d", SDL_GetTicks(), index);

    // Priority
    int priority = (index % 2 == 0) ? -1 : 999;

    m_shatters.push_back(std::make_unique<WindowShatter>(title, spawnPos, velocity, size, priority));
}

void WindowShatterManager::Clear()
{
    m_shatters.clear();
}