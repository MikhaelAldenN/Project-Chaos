#include "WindowShatter.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <random>
#include <cmath>
#include <algorithm> // WAJIB: Untuk std::remove_if dan std::remove
#include <windows.h> // Untuk GetSystemMetrics

WindowShatter::WindowShatter(
    const char* title,
    DirectX::XMFLOAT2 startPos,
    DirectX::XMFLOAT2 initialVelocity,
    int initialWidth,
    int initialHeight,
    float deceleration,
    float shrinkRate)
    : shrinkRate(shrinkRate)
    , currentWidth(static_cast<float>(initialWidth))
    , currentHeight(static_cast<float>(initialHeight))
    , markedForDestroy(false)
{
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    window = WindowManager::Instance().CreateGameWindow(title, initialWidth, initialHeight);

    SDL_SetWindowPosition(window->GetSDLWindow(),
        static_cast<int>(startPos.x),
        static_cast<int>(startPos.y));

    window->SetPriority(999);
    window->SetDraggable(false);

    physics.velocity = initialVelocity;
    physics.deceleration = deceleration;
    physics.gravity = 0.0f;
    physics.bounceDamping = 0.7f;
    physics.bounceCount = 0;
    physics.maxBounces = 3;
    physics.angularVelocity = static_cast<float>(rand() % 200 - 100) * 2.0f;
}

WindowShatter::~WindowShatter()
{
    if (window)
    {
        WindowManager::Instance().DestroyWindow(window);
        window = nullptr;
    }
}

void WindowShatter::Update(float dt)
{
    if (markedForDestroy) return;

    // Jalankan simulasi fisika dan perubahan ukuran
    UpdatePhysics(dt);
    UpdateSize(dt);
    CheckBounds();

    // --- KONDISI 1: Cek Kecepatan (Speed) ---
    // Menggunakan kuadrat kecepatan (v^2) untuk efisiensi
    // $v^2 = vx^2 + vy^2$
    float velocitySq = (physics.velocity.x * physics.velocity.x) +
        (physics.velocity.y * physics.velocity.y);

    // Anggap berhenti jika kecepatan di bawah 5 pixel/detik
    const float STOP_THRESHOLD_SQ = 5.0f * 5.0f;
    bool isStopped = (velocitySq < STOP_THRESHOLD_SQ);

    // --- KONDISI 2: Cek Ukuran (Size) ---
    // Mengacu pada MIN_SIZE yang didefinisikan di WindowShatter.h
    bool isTooSmall = (currentWidth <= MIN_SIZE || currentHeight <= MIN_SIZE);

    // --- EKSEKUSI PENGHANCURAN ---
    // Window ditandai untuk dihancurkan jika berhenti ATAU sudah mencapai ukuran minimum
    if (isStopped || isTooSmall)
    {
        markedForDestroy = true;
    }
}

void WindowShatter::UpdatePhysics(float dt)
{
    //physics.velocity.y += physics.gravity * dt;
    float decelFactor = physics.deceleration;

    if (physics.bounceCount > physics.maxBounces)
    {
        decelFactor *= 0.9f;
    }

    physics.velocity.x *= decelFactor;
    physics.velocity.y *= decelFactor;

    int currentX, currentY;
    SDL_GetWindowPosition(window->GetSDLWindow(), &currentX, &currentY);

    float newX = static_cast<float>(currentX) + physics.velocity.x * dt;
    float newY = static_cast<float>(currentY) + physics.velocity.y * dt;

    SDL_SetWindowPosition(window->GetSDLWindow(),
        static_cast<int>(newX),
        static_cast<int>(newY));
}

void WindowShatter::UpdateSize(float dt)
{
    // 1. Simpan ukuran lama untuk perhitungan centering
    float oldWidth = currentWidth;
    float oldHeight = currentHeight;

    // 2. Hitung pengurangan ukuran
    float shrinkAmount = shrinkRate * dt;
    currentWidth -= shrinkAmount;
    currentHeight -= shrinkAmount;

    // 3. Clamp ke minimum (agar tidak menghilang tiba-tiba sebelum di-destroy)
    if (currentWidth < MIN_SIZE) currentWidth = MIN_SIZE;
    if (currentHeight < MIN_SIZE) currentHeight = MIN_SIZE;

    // 4. Hitung berapa banyak ukuran sebenarnya berkurang (setelah clamp)
    float deltaW = oldWidth - currentWidth;
    float deltaH = oldHeight - currentHeight;

    // 5. Ambil posisi window saat ini
    int curX, curY;
    SDL_GetWindowPosition(window->GetSDLWindow(), &curX, &curY);

    // 6. Hitung posisi baru agar tetap di tengah (Centering Logic)
    float newX = static_cast<float>(curX) + (deltaW / 2.0f);
    float newY = static_cast<float>(curY) + (deltaH / 2.0f);

    // --- BAGIAN KRUSIAL ---

    // 7. Ubah UKURAN FISIK jendela di desktop (Agar bingkai jendela mengecil)
    SDL_SetWindowSize(window->GetSDLWindow(),
        static_cast<int>(currentWidth),
        static_cast<int>(currentHeight));

    // 8. Ubah POSISI jendela agar tetap sinkron dengan pengecilan (Centering)
    SDL_SetWindowPosition(window->GetSDLWindow(),
        static_cast<int>(newX),
        static_cast<int>(newY));

    // 9. Update internal engine (untuk resolusi/viewport DirectX)
    window->Resize(static_cast<int>(currentWidth), static_cast<int>(currentHeight));
}

void WindowShatter::CheckBounds()
{
    int x, y;
    SDL_GetWindowPosition(window->GetSDLWindow(), &x, &y);
    int w = window->GetWidth();
    int h = window->GetHeight();

    bool bounced = false;
    if (x <= 0) { x = 0; ApplyBounce(true); bounced = true; }
    else if (x + w >= screenWidth) { x = screenWidth - w; ApplyBounce(true); bounced = true; }

    if (y <= 0) { y = 0; ApplyBounce(false); bounced = true; }
    else if (y + h >= screenHeight) { y = screenHeight - h; ApplyBounce(false); bounced = true; }

    if (bounced)
    {
        SDL_SetWindowPosition(window->GetSDLWindow(), x, y);
        physics.bounceCount++;
    }
}

void WindowShatter::ApplyBounce(bool horizontal)
{
    if (horizontal) physics.velocity.x = -physics.velocity.x * physics.bounceDamping;
    else physics.velocity.y = -physics.velocity.y * physics.bounceDamping;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
    physics.velocity.x += dist(gen);
}

void WindowShatterManager::SpawnShatterExplosion(
    DirectX::XMFLOAT2 centerPos,
    int count,
    float minSpeed,
    float maxSpeed)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    // Hitung berapa derajat jatah untuk setiap pecahan agar rata
    float angleStep = 360.0f / static_cast<float>(count);

    std::uniform_real_distribution<float> speedDist(minSpeed, maxSpeed);
    std::uniform_real_distribution<float> sizeDist(200.0f, 300.0f);
    std::uniform_real_distribution<float> decelDist(0.92f, 0.98f); // Nilai decel lebih kuat agar cepat berhenti
    std::uniform_real_distribution<float> shrinkDist(40.0f, 60.0f);

    // Jitter: variasi kecil agar tidak terlihat seperti lingkaran sempurna (bintang)
    // Kita beri variasi sekitar 40% dari ukuran satu slice
    std::uniform_real_distribution<float> jitterDist(-angleStep * 0.4f, angleStep * 0.4f);

    for (int i = 0; i < count; ++i)
    {
        // Tentukan sudut dasar untuk pecahan ke-i
        float baseAngle = i * angleStep;

        // Tambahkan sedikit jitter agar tetap terasa acak tapi tetap merata
        float finalAngle = baseAngle + jitterDist(gen);
        float angleRad = DirectX::XMConvertToRadians(finalAngle);

        float speed = speedDist(gen);
        DirectX::XMFLOAT2 velocity = {
            cosf(angleRad) * speed,
            sinf(angleRad) * speed
        };

        int size = static_cast<int>(sizeDist(gen));
        float decel = decelDist(gen);
        float shrink = shrinkDist(gen);

        // Spawn position sedikit random di sekitar center
        std::uniform_real_distribution<float> offsetDist(-20.0f, 20.0f);
        DirectX::XMFLOAT2 spawnPos = {
            centerPos.x + offsetDist(gen),
            centerPos.y + offsetDist(gen)
        };

        char title[64];
        sprintf_s(title, "Shatter_%d", i);

        auto shatter = std::make_unique<WindowShatter>(
            title,
            spawnPos,
            velocity,
            size, size,
            decel,
            shrink // ShrinkRate 0 jika kamu ingin ukuran tetap sampai berhenti
        );

        shatters.push_back(std::move(shatter));
    }
}

void WindowShatterManager::Update(float dt)
{
    for (auto& shatter : shatters)
    {
        shatter->Update(dt);
    }

    shatters.erase(
        std::remove_if(shatters.begin(), shatters.end(),
            [](const std::unique_ptr<WindowShatter>& s) {
                return s->ShouldDestroy();
            }),
        shatters.end()
    );
}

void WindowShatterManager::Clear()
{
    shatters.clear();
}
// JANGAN ADA KURUNG KURAWAL TUTUP DI SINI