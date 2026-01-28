#include "WindowTrackingSystem.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <windows.h> // Untuk GetSystemMetrics
#include <cmath>

using namespace DirectX;

WindowTrackingSystem::WindowTrackingSystem()
{
}

WindowTrackingSystem::~WindowTrackingSystem()
{
    ClearAll();
}

void WindowTrackingSystem::ClearAll()
{
    // Clean up windows via WindowManager
    for (auto& tracked : m_trackedWindows)
    {
        if (tracked->window)
        {
            WindowManager::Instance().DestroyWindow(tracked->window);
        }
    }
    m_trackedWindows.clear();
    m_windowLookup.clear();
}

bool WindowTrackingSystem::AddTrackedWindow(const TrackedWindowConfig& config, std::function<DirectX::XMFLOAT3()> getTargetPos)
{
    // 1. Create Window via Singleton Manager
    GameWindow* window = WindowManager::Instance().CreateGameWindow(
        config.title.c_str(),
        config.width,
        config.height
    );

    if (!window) return false;

    window->SetPriority(config.priority);
    // Hardcoded logic: Player window usually shouldn't be draggable by mouse
    if (config.name == "player") window->SetDraggable(false);

    // 2. Create Camera for this window
    auto camera = std::make_shared<Camera>();
    window->SetCamera(camera.get());

    // 3. Setup Tracked Object
    auto tracked = std::make_unique<TrackedWindow>();
    tracked->name = config.name;
    tracked->window = window;
    tracked->camera = camera;
    tracked->trackingOffset = config.trackingOffset;
    tracked->getTargetPositionFunc = getTargetPos;

    // 4. Initial Position Setup
    if (getTargetPos)
    {
        XMFLOAT3 initialPos = getTargetPos();
        float screenX, screenY;
        WorldToScreenPos(initialPos, screenX, screenY);

        tracked->state.targetX = screenX - (window->GetWidth() * 0.5f);
        tracked->state.targetY = screenY - (window->GetHeight() * 0.5f);
        tracked->state.actualX = static_cast<int>(roundf(tracked->state.targetX));
        tracked->state.actualY = static_cast<int>(roundf(tracked->state.targetY));

        SDL_SetWindowPosition(window->GetSDLWindow(), tracked->state.actualX, tracked->state.actualY);
    }

    m_windowLookup[config.name] = tracked.get();
    m_trackedWindows.push_back(std::move(tracked));

    return true;
}

TrackedWindow* WindowTrackingSystem::GetTrackedWindow(const std::string& name)
{
    auto it = m_windowLookup.find(name);
    if (it != m_windowLookup.end()) return it->second;
    return nullptr;
}

void WindowTrackingSystem::Update(float dt)
{
    // 1. Update Screen Cache (Resolusi layar mungkin berubah)
    m_cacheUpdateTimer += dt;
    if (m_cacheUpdateTimer >= 1.0f)
    {
        m_cachedScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        m_cachedScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        m_cacheUpdateTimer = 0.0f;
    }

    // 2. Update Semua Window
    for (auto& tracked : m_trackedWindows)
    {
        UpdateSingleWindow(dt, *tracked);
    }
}

void WindowTrackingSystem::UpdateSingleWindow(float dt, TrackedWindow& tracked)
{
    if (!tracked.window || !tracked.camera || !tracked.getTargetPositionFunc)
        return;

    // A. Hitung Target Posisi
    XMFLOAT3 targetWorldPos = tracked.getTargetPositionFunc();
    targetWorldPos.x += tracked.trackingOffset.x;
    targetWorldPos.y += tracked.trackingOffset.y;
    targetWorldPos.z += tracked.trackingOffset.z;

    float targetScreenX, targetScreenY;
    WorldToScreenPos(targetWorldPos, targetScreenX, targetScreenY);

    float destX = targetScreenX - (tracked.window->GetWidth() * 0.5f);
    float destY = targetScreenY - (tracked.window->GetHeight() * 0.5f);

    // B. Smooth Movement (Lerp)
    float t = min(m_followSpeed * dt, 1.0f);
    tracked.state.targetX += (destX - tracked.state.targetX) * t;
    tracked.state.targetY += (destY - tracked.state.targetY) * t;

    // C. Apply ke SDL Window (dengan threshold optimization)
    int newX = static_cast<int>(roundf(tracked.state.targetX));
    int newY = static_cast<int>(roundf(tracked.state.targetY));

    int deltaX = abs(newX - tracked.state.actualX);
    int deltaY = abs(newY - tracked.state.actualY);

    if (deltaX >= 2 || deltaY >= 2) // Threshold 2 pixel
    {
        SDL_SetWindowPosition(tracked.window->GetSDLWindow(), newX, newY);
        tracked.state.actualX = newX;
        tracked.state.actualY = newY;
    }

    // D. Update Camera Projection (Agar perspektif tetap benar)
    UpdateOffCenterProjection(tracked.camera.get(), tracked.window, GetUnifiedCameraHeight());
}

// =========================================================
// MATH HELPERS
// =========================================================
void WindowTrackingSystem::GetScreenDimensions(int& outWidth, int& outHeight)
{
    if (m_cachedScreenWidth > 0) {
        outWidth = m_cachedScreenWidth;
        outHeight = m_cachedScreenHeight;
    }
    else {
        outWidth = GetSystemMetrics(SM_CXSCREEN);
        outHeight = GetSystemMetrics(SM_CYSCREEN);
        m_cachedScreenWidth = outWidth;
        m_cachedScreenHeight = outHeight;
    }
}

void WindowTrackingSystem::WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY)
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

    // Konversi sederhana: Tengah layar + (Posisi World * Ratio)
    // Note: Z world menjadi Y layar (negatif) karena coordinate system game ini
    outScreenX = (screenW * 0.5f) + (worldPos.x * m_pixelToUnitRatio);
    outScreenY = (screenH * 0.5f) - (worldPos.z * m_pixelToUnitRatio);
}

float WindowTrackingSystem::GetUnifiedCameraHeight()
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);
    float halfFovTan = tanf(XMConvertToRadians(m_fov) * 0.5f);
    // Rumus trigonometri untuk mencari ketinggian kamera agar 1 unit world = X pixel layar
    return (screenH * 0.5f) / (m_pixelToUnitRatio * halfFovTan);
}

void WindowTrackingSystem::UpdateOffCenterProjection(Camera* targetCam, GameWindow* targetWin, float camHeight)
{
    int screenW, screenH;
    GetScreenDimensions(screenW, screenH);

    targetCam->SetPosition(0.0f, camHeight, 0.0f);
    targetCam->LookAt({ 0.0f, 0.0f, 0.0f });

    int winX, winY, winW, winH;
    SDL_GetWindowPosition(targetWin->GetSDLWindow(), &winX, &winY);
    SDL_GetWindowSize(targetWin->GetSDLWindow(), &winW, &winH);

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(XMConvertToRadians(m_fov) * 0.5f);

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * ((float)screenW / screenH);

    // Hitung frustum offset berdasarkan posisi window di layar fisik
    double screenWd = (double)screenW;
    double screenHd = (double)screenH;

    float l = (float)((winX / screenWd) * 2.0 - 1.0);
    float r = (float)(((winX + winW) / screenWd) * 2.0 - 1.0);
    float t = (float)(1.0 - (winY / screenHd) * 2.0);
    float b = (float)(1.0 - ((winY + winH) / screenHd) * 2.0);

    targetCam->SetOffCenterProjection(
        l * halfWidth, r * halfWidth,
        b * halfHeight, t * halfHeight,
        nearZ, farZ
    );
}