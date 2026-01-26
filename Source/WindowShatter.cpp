#include "WindowShatter.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>
#include <windows.h> // GetSystemMetrics

WindowShatter::WindowShatter(
    const char* title, DirectX::XMFLOAT2 startPos, DirectX::XMFLOAT2 initialVelocity,
    int initialWidth, int initialHeight, float deceleration, float shrinkRate, int priority) : m_shrinkRate(shrinkRate)
    , m_currentWidth(static_cast<float>(initialWidth))
    , m_currentHeight(static_cast<float>(initialHeight))
{

    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 1. JANGAN PANGGIL WindowManager::CreateGameWindow DI SINI.
    m_isNativeWindow = false;
    m_window = nullptr;

    // 2. Simpan posisi awal sebagai posisi 3D di dunia game (Konversi dari screen ke world)
    m_fakeWorldPos = { startPos.x, 0.0f, startPos.y }; // Asumsi Y adalah atas/bawah jika top-down

    // Setup Fisika awal
    m_physics.velocity = initialVelocity;
    m_physics.deceleration = deceleration;
    m_physics.angularVelocity = static_cast<float>(rand() % 200 - 100) * 2.0f;
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
        // --- LOGIKA SAAT MASIH JADI OBJEK 3D (FAKE) ---
        // 1. Update posisi di dunia 3D
        m_fakeWorldPos.x += m_physics.velocity.x * dt;
        m_fakeWorldPos.z += m_physics.velocity.y * dt; // Sesuaikan dengan sumbu Z/Y game Anda

        // 2. Cek apakah sudah menyentuh batas layar
        float screenX, screenY;
        // Asumsi Anda punya akses ke fungsi WorldToScreenPos dari sini atau Scene
        ConvertWorldToScreen(m_fakeWorldPos, screenX, screenY);

        if (screenX <= 0 || screenX >= m_screenWidth || screenY <= 0 || screenY >= m_screenHeight)
        {
            // Saat keluar batas, BERUBAH MENJADI WINDOW ASLI
            TransitionToNativeWindow();
        }
    }
    else
    {
        // --- LOGIKA SAAT SUDAH JADI WINDOW ASLI (NATIVE) ---
        UpdatePhysics(dt); // Kode SDL_SetWindowPosition Anda yang asli
        UpdateSize(dt);
        CheckBounds();
    }
}

void WindowShatter::UpdatePhysics(float dt)
{
    float decelFactor = m_physics.deceleration;

    // Apply extra friction if bounced too many times
    if (m_physics.bounceCount > m_physics.maxBounces)
    {
        decelFactor *= 0.9f;
    }

    m_physics.velocity.x *= decelFactor;
    m_physics.velocity.y *= decelFactor;

    int curX, curY;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &curX, &curY);

    float newX = static_cast<float>(curX) + m_physics.velocity.x * dt;
    float newY = static_cast<float>(curY) + m_physics.velocity.y * dt;

    int nextX = static_cast<int>(newX);
    int nextY = static_cast<int>(newY);

    if (nextX != curX || nextY != curY)
    {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), nextX, nextY);
    }

    //SDL_SetWindowPosition(m_window->GetSDLWindow(), static_cast<int>(newX), static_cast<int>(newY));
}

void WindowShatter::UpdateSize(float dt)
{
    float oldWidth = m_currentWidth;
    float oldHeight = m_currentHeight;

    // Apply shrink
    float shrinkAmount = m_shrinkRate * dt;
    m_currentWidth = max(MIN_SIZE, m_currentWidth - shrinkAmount);
    m_currentHeight = max(MIN_SIZE, m_currentHeight - shrinkAmount);

    // Centering logic
    float deltaW = oldWidth - m_currentWidth;
    float deltaH = oldHeight - m_currentHeight;

    int curX, curY;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &curX, &curY);

    float newX = static_cast<float>(curX) + (deltaW * 0.5f);
    float newY = static_cast<float>(curY) + (deltaH * 0.5f);

    // Apply physical size and position updates
    SDL_SetWindowSize(m_window->GetSDLWindow(), static_cast<int>(m_currentWidth), static_cast<int>(m_currentHeight));
    SDL_SetWindowPosition(m_window->GetSDLWindow(), static_cast<int>(newX), static_cast<int>(newY));
    m_window->Resize(static_cast<int>(m_currentWidth), static_cast<int>(m_currentHeight));
}

void WindowShatter::CheckBounds()
{
    int x, y;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &x, &y);
    int w = m_window->GetWidth();
    int h = m_window->GetHeight();

    bool bounced = false;
    if (x <= 0) { x = 0; ApplyBounce(true); bounced = true; }
    else if (x + w >= m_screenWidth) { x = m_screenWidth - w; ApplyBounce(true); bounced = true; }

    if (y <= 0) { y = 0; ApplyBounce(false); bounced = true; }
    else if (y + h >= m_screenHeight) { y = m_screenHeight - h; ApplyBounce(false); bounced = true; }

    if (bounced)
    {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), x, y);
        m_physics.bounceCount++;
    }
}

void WindowShatter::ApplyBounce(bool horizontal)
{
    if (horizontal) m_physics.velocity.x = -m_physics.velocity.x * m_physics.bounceDamping;
    else m_physics.velocity.y = -m_physics.velocity.y * m_physics.bounceDamping;

    // Apply small random deflection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
    m_physics.velocity.x += dist(gen);
}

void WindowShatterManager::SpawnShatterExplosion(DirectX::XMFLOAT2 centerPos, int count, float minSpeed, float maxSpeed)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    float angleStep = 360.0f / static_cast<float>(count);

    std::vector<int> priorities(count);
    int halfCount = count / 2;
    for (int i = 0; i < count; ++i)
    {
        // Set -1 untuk Top Priority (di atas Player), 999 untuk Bottom (di belakang Scene)
        priorities[i] = (i < halfCount) ? -1 : 999;
    }
    // Acak urutannya agar tidak selalu window pertama yang di atas
    std::shuffle(priorities.begin(), priorities.end(), gen);

    // Distribution setup
    std::uniform_real_distribution<float> speedDist(minSpeed, maxSpeed);
    std::uniform_real_distribution<float> sizeDist(200.0f, 300.0f);
    std::uniform_real_distribution<float> decelDist(0.92f, 0.98f);
    std::uniform_real_distribution<float> shrinkDist(40.0f, 60.0f);
    std::uniform_real_distribution<float> jitterDist(-angleStep * 0.4f, angleStep * 0.4f);
    std::uniform_real_distribution<float> offsetDist(-20.0f, 20.0f);

    for (int i = 0; i < count; ++i)
    {
        float angleRad = DirectX::XMConvertToRadians((i * angleStep) + jitterDist(gen));
        float speed = speedDist(gen);

        DirectX::XMFLOAT2 velocity = { cosf(angleRad) * speed, sinf(angleRad) * speed };
        DirectX::XMFLOAT2 spawnPos = { centerPos.x + offsetDist(gen), centerPos.y + offsetDist(gen) };

        int size = static_cast<int>(sizeDist(gen));
        float decel = decelDist(gen);
        float shrink = shrinkDist(gen);

        char title[32];
        sprintf_s(title, "Shatter_%d", i);

        m_shatters.push_back(std::make_unique<WindowShatter>(title, spawnPos, velocity, size, size, decel, shrink, priorities[i]));
    }
}

void WindowShatterManager::Update(float dt)
{
    for (auto& shatter : m_shatters) shatter->Update(dt);

    m_shatters.erase(std::remove_if(m_shatters.begin(), m_shatters.end(),
        [](const std::unique_ptr<WindowShatter>& s) { return s->ShouldDestroy(); }),
        m_shatters.end());
}

void WindowShatterManager::Clear()
{
    m_shatters.clear();
}
void WindowShatterManager::SpawnSingleShatter(DirectX::XMFLOAT2 centerPos, int index)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    // Hitung sudut ledakan berdasarkan index (misal total 8 pecahan)
    float totalShatters = 8.0f;
    float angleStep = 360.0f / totalShatters;

    // Beri sedikit random jitter pada sudutnya
    std::uniform_real_distribution<float> jitterDist(-15.0f, 15.0f);
    float angleRad = DirectX::XMConvertToRadians((index * angleStep) + jitterDist(gen));

    // Randomisasi Kecepatan dan Ukuran
    std::uniform_real_distribution<float> speedDist(300.0f, 700.0f);
    std::uniform_real_distribution<float> sizeDist(200.0f, 300.0f);
    std::uniform_real_distribution<float> offsetDist(-10.0f, 10.0f);

    float speed = speedDist(gen);
    DirectX::XMFLOAT2 velocity = { cosf(angleRad) * speed, sinf(angleRad) * speed };
    DirectX::XMFLOAT2 spawnPos = { centerPos.x + offsetDist(gen), centerPos.y + offsetDist(gen) };

    int size = static_cast<int>(sizeDist(gen));

    // Beri nama window yang unik
    char title[32];
    sprintf_s(title, "Shatter_%d", index);

    // Tentukan prioritas (Sebagian di depan player (-1), sebagian di belakang (999))
    int priority = (index % 2 == 0) ? -1 : 999;

    // Masukkan 1 pecahan ke dalam vector manager
    m_shatters.push_back(std::make_unique<WindowShatter>(
        title, spawnPos, velocity, size, size,
        0.95f, 50.0f, priority // Deceleration & Shrink Rate
    ));
}

void WindowShatter::TransitionToNativeWindow()
{
    // 1. Hitung posisi layar terakhir
    float screenX, screenY;
    ConvertWorldToScreen(m_fakeWorldPos, screenX, screenY);

    // 2. Spawn OS Window SEKARANG (Hanya 1 window per frame maksimal karena tersebar)
    m_window = WindowManager::Instance().CreateGameWindow("Shatter", static_cast<int>(m_currentWidth), static_cast<int>(m_currentHeight));

    // 3. Set posisi SDL di perbatasan tempat dia keluar
    SDL_SetWindowPosition(m_window->GetSDLWindow(), static_cast<int>(screenX), static_cast<int>(screenY));

    // 4. Inisialisasi Kamera Top-Down untuk window ini
    m_camera = std::make_shared<Camera>();
    m_camera->SetRotation(90.0f, 0.0f, 0.0f);
    m_window->SetCamera(m_camera.get());

    m_isNativeWindow = true; // Flag diubah, frame berikutnya pakai logika Native
}

void WindowShatter::ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY) const
{
    // Titik pusat layar adalah koordinat Dunia (0,0)
    outScreenX = (m_screenWidth * 0.5f) + (worldPos.x * PIXEL_TO_UNIT_RATIO);
    outScreenY = (m_screenHeight * 0.5f) - (worldPos.z * PIXEL_TO_UNIT_RATIO); // Z dibalik untuk sumbu Y layar
}