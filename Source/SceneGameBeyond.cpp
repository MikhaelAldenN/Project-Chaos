#include <SDL3/SDL.h>
#include <cmath>
#include <windows.h>
#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGameBeyond.h"
#include "WindowManager.h"
#include "Framework.h"
#include "Enemy.h"
#include <unordered_set>
#include <imgui.h>

using namespace DirectX;

// Definisi Konstanta Lokal
#define FIELD_OF_VIEW 60.0f
#define DEFERRED_INIT_TIME 0.2f
#define CACHE_REFRESH_INTERVAL 1.0f
#define PRIORITY_ENFORCE_INTERVAL 2.0f
#define PIXEL_TO_UNIT_RATIO 40.0f

// =========================================================
// CONSTRUCTOR
// =========================================================
SceneGameBeyond::SceneGameBeyond()
{
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    float screenW = 1920.0f;
    float screenH = 1080.0f;

    // 0. Setup Window System
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetPixelToUnitRatio(PIXEL_TO_UNIT_RATIO);
    m_windowSystem->SetFOV(FIELD_OF_VIEW);

    float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();

    // 1. Setup Main Camera
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(FIELD_OF_VIEW), screenW / screenH, 0.1f, 1000.0f);
    m_mainCamera->SetPosition(0.0f, unifiedHeight, 0.0f);
    m_mainCamera->LookAt({ 0, 0, 0 });

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // 2. Setup Assets
    m_player = std::make_unique<Player>();
    m_player->SetInvertControls(true);
    m_player->SetPosition(0.0f, 0.0f, -8.0f);

    m_blockManager = std::make_unique<BlockManager>();
    m_blockManager->Initialize(m_player.get());
    m_blockManager->ClearBlocks();
    m_blockManager->ActivateFormationMode();

    m_blockManager->shieldSettings.Enabled = true;
    m_blockManager->shieldSettings.MaxTetherDistance = 8.0f; // Jarak tameng agak jauh dikit
    m_blockManager->shootSettings.ProjectileSpeed = 20.0f;   // Kecepatan peluru

    for (int i = 0; i < 20; ++i) m_blockManager->SpawnAllyBlock(m_player.get());

    m_boss = std::make_unique<Boss>();

    if (m_boss && m_player)
    {
        m_boss->SetPlayer(m_player.get());
    }

    m_enemyManager = std::make_unique<EnemyManager>();

    // 3. Sub Camera
    m_subCamera = std::make_shared<Camera>();
    m_subCamera->SetPerspectiveFov(XMConvertToRadians(60), 4.0f / 3.0f, 0.1f, 1000.0f);
    m_subCamera->SetPosition(5, 5, 5);
    m_subCamera->LookAt({ 0, 0, 0 });

    // 4. Setup Primitive Renderer
    auto device = Graphics::Instance().GetDevice();
    m_primitive2D = std::make_unique<Primitive>(device);

    if (m_boss && m_enemyManager)
    {
        m_boss->SetEnemyManager(m_enemyManager.get()); // Pass pointer agar Boss bisa spawn musuh
    }

    // [BARU] 1. Setup Item Manager (Agar item drop berfungsi)
    m_itemManager = std::make_unique<ItemManager>();
    m_itemManager->Initialize(Graphics::Instance().GetDevice());

    // [BARU] 2. Setup Collision Manager
    m_collisionManager = std::make_unique<CollisionManager>();

    // [UPDATE] Tambahkan m_boss.get() di parameter terakhir
    m_collisionManager->Initialize(
        m_player.get(),
        nullptr,             // Stage null
        m_blockManager.get(),
        m_enemyManager.get(),
        m_itemManager.get(),
        m_boss.get()         // <--- INI PARAMETER BARU
    );

    // [OPSIONAL] Callback efek visual saat Player kena Hit
    m_collisionManager->SetOnPlayerHitCallback([this]() {
        if (m_player) {
            // Trigger efek kaca pecah kecil saat player kena damage
            auto pPos = m_player->GetPosition();
            WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 4);
        }
        });

}

// =========================================================
// DESTRUCTOR
// =========================================================
SceneGameBeyond::~SceneGameBeyond()
{
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    WindowShatterManager::Instance().Clear();
    CameraController::Instance().ClearCamera();
}

// =========================================================
// INITIALIZE SUB WINDOWS
// =========================================================
void SceneGameBeyond::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player || !m_boss) return;

    // 1. Player Tracking Window
    m_windowSystem->AddTrackedWindow(
        { "player", "Player View", 300, 300, 1, { 0.0f, 0.0f, 0.0f } },

        // 1. LAMBDA POSISI (Center)
        [this]() -> DirectX::XMFLOAT3 {
            if (!m_player) return DirectX::XMFLOAT3(0, 0, 0);

            auto pPos = m_player->GetPosition();

            // Safe Zone 150px
            float safeMarginPixel = 150.0f;
            float safeMarginWorld = safeMarginPixel / PIXEL_TO_UNIT_RATIO;

            float minX = pPos.x - safeMarginWorld;
            float maxX = pPos.x + safeMarginWorld;
            float minZ = pPos.z - safeMarginWorld;
            float maxZ = pPos.z + safeMarginWorld;

            if (m_blockManager && m_blockManager->IsShieldActive())
            {
                const auto& blocks = m_blockManager->GetBlocks();
                for (const auto& block : blocks)
                {
                    // TAMBAHAN: && !block->IsProjectile()
                    // Jangan hitung blok yang sedang ditembakkan!
                    if (block && block->IsActive() && !block->IsProjectile())
                    {
                        auto bPos = block->GetMovement()->GetPosition();
                        if (bPos.x < minX) minX = bPos.x;
                        if (bPos.x > maxX) maxX = bPos.x;
                        if (bPos.z < minZ) minZ = bPos.z;
                        if (bPos.z > maxZ) maxZ = bPos.z;
                    }
                }
            }

            float centerX = (minX + maxX) * 0.5f;
            float centerZ = (minZ + maxZ) * 0.5f;

            return DirectX::XMFLOAT3(centerX, 0.0f, centerZ);
        },

        // 2. LAMBDA UKURAN (Size)
        [this]() -> DirectX::XMFLOAT2 {
            if (!m_player) return DirectX::XMFLOAT2(300, 300);

            auto pPos = m_player->GetPosition();
            float safeMarginPixel = 150.0f;
            float safeMarginWorld = safeMarginPixel / PIXEL_TO_UNIT_RATIO;

            float minX = pPos.x - safeMarginWorld;
            float maxX = pPos.x + safeMarginWorld;
            float minZ = pPos.z - safeMarginWorld;
            float maxZ = pPos.z + safeMarginWorld;

            if (m_blockManager && m_blockManager->IsShieldActive())
            {
                const auto& blocks = m_blockManager->GetBlocks();
                for (const auto& block : blocks)
                {
                    // TAMBAHAN: && !block->IsProjectile()
                    // Hanya hitung blok yang menempel di shield/formasi
                    if (block && block->IsActive() && !block->IsProjectile())
                    {
                        auto bPos = block->GetMovement()->GetPosition();
                        if (bPos.x < minX) minX = bPos.x;
                        if (bPos.x > maxX) maxX = bPos.x;
                        if (bPos.z < minZ) minZ = bPos.z;
                        if (bPos.z > maxZ) maxZ = bPos.z;
                    }
                }
            }

            float rangeX = maxX - minX;
            float rangeZ = maxZ - minZ;
            float extraPadding = 50.0f;

            float w = (rangeX * PIXEL_TO_UNIT_RATIO) + extraPadding;
            float h = (rangeZ * PIXEL_TO_UNIT_RATIO) + extraPadding;

            return DirectX::XMFLOAT2(w, h);
        }
    );

    // 2. Boss Monitor 1 (Main Head)
    if (m_boss->HasPart("monitor1"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor1", "Boss Monitor", 340, 340, 0, { -0.3f, 0.0f, 2.1f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitorVisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 3. CPU (Body)
    if (m_boss->HasPart("cpu"))
    {
        m_windowSystem->AddTrackedWindow(
            { "cpu", "System Unit", 186, 370, 0, { -8.2f, 0.0f, 4.0f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetCPUVisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 4. Monitor 2 (Side Left)
    if (m_boss->HasPart("monitor2"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor2", "Side Monitor L", 240, 210, 4, { 0.5f, 0.0f, -0.3f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitor2VisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    // 5. Monitor 3 (Side Right)
    if (m_boss->HasPart("monitor3"))
    {
        m_windowSystem->AddTrackedWindow(
            { "monitor3", "Side Monitor R", 200, 200, 3, { 0.8f, 0.0f, 1.2f } },
            [this]() -> DirectX::XMFLOAT3 {
                if (m_boss) {
                    auto pos = m_boss->GetMonitor3VisualPos();
                    return XMFLOAT3{ pos.x, 0.0f, pos.z };
                }
                return DirectX::XMFLOAT3(0, 0, 0);
            }
        );
    }

    if (m_boss->HasPart("antenna"))
    {
        m_windowSystem->AddTrackedWindow(
            // Config Dasar (Size dasar 220x500)
            { "antenna", "Signal Uplink", 220, 500, 5, { 0.0f, 0.0f, 0.0f } },

            // 1. LAMBDA POSISI (Model Pos + Offset ImGui)
            [this]() -> DirectX::XMFLOAT3 {
                if (!m_boss) return { 0,0,0 };

                // Ambil posisi asli model (yang sedang animasi slide)
                auto pos = m_boss->GetAntennaVisualPos();

                bool isMirrored = (pos.x < -1.0f);

                if (isMirrored)
                {
                    // === SETTING KHUSUS MIRRORED (KIRI) ===
                    // Geser ke kanan dikit (X +) agar pas
                    pos.x += 1.8f; // <--- UBAH ANGKA INI (Geser Kanan)

                    // Kamu juga bisa atur Y atau Z beda kalau perlu
                    pos.y += m_antennaTrackOffset.y;
                    pos.z += m_antennaTrackOffset.z;
                }
                else
                {
                    // === SETTING NORMAL (KANAN) ===
                    // Pakai settingan standar (bisa dari Slider ImGui)
                    pos.x += m_antennaTrackOffset.x;
                    pos.y += m_antennaTrackOffset.y;
                    pos.z += m_antennaTrackOffset.z;
                }
                return { pos.x, pos.y, pos.z };
            },

            // 2. LAMBDA SIZE (Base Size + Offset ImGui)
            [this]() -> DirectX::XMFLOAT2 {
                float baseW = 220.0f;
                float baseH = 500.0f;

                // Tambahkan offset, tapi jangan sampai negatif
                float w = max(50.0f, baseW + m_antennaSizeOffset.x);
                float h = max(50.0f, baseH + m_antennaSizeOffset.y);

                return DirectX::XMFLOAT2(w, h);
            }
        );
    }



    WindowManager::Instance().EnforceWindowPriorities();
    m_isWindowsInitialized = true;
}

void SceneGameBeyond::UpdateEnemyWindows()
{
    if (!m_enemyManager || !m_windowSystem) return;

    const auto& enemies = m_enemyManager->GetEnemies();
    size_t enemyCount = enemies.size();

    // 1. SPAWN WINDOW BARU (Untuk musuh yang ada)
    for (size_t i = 0; i < enemyCount; ++i)
    {
        std::string winID = "enemy_view_" + std::to_string(i);

        if (m_windowSystem->GetTrackedWindow(winID) == nullptr)
        {
            // ... (Kode AddTrackedWindow yang kamu miliki sebelumnya) ...
            m_windowSystem->AddTrackedWindow(
                { winID, "Enemy Signal " + std::to_string(i), 150, 150, 2, { 0.0f, 0.0f, 0.0f } },
                [this, i]() -> DirectX::XMFLOAT3 {
                    if (!m_enemyManager) return { 0,0,0 };
                    const auto& curEnemies = m_enemyManager->GetEnemies();
                    if (i >= curEnemies.size() || !curEnemies[i]) return { 0,0,0 };

                    auto pos = curEnemies[i]->GetPosition();
                    pos.x += m_enemyTrackOffset.x;
                    pos.y += m_enemyTrackOffset.y;
                    pos.z += m_enemyTrackOffset.z;
                    return pos;
                },
                [this]() -> DirectX::XMFLOAT2 {
                    float w = 150 + m_enemySizeOffset.x;
                    float h = 150 + m_enemySizeOffset.y;
                    return DirectX::XMFLOAT2(max(50.0f, w), max(50.0f, h));
                }
            );
        }
    }

    // 2. CLEANUP WINDOW (Untuk musuh yang sudah mati)
    // Kita cek window dengan ID "enemy_view_X". Jika X >= enemyCount, berarti musuhnya sudah tidak ada.
    // Kita looping agak lebih dari enemyCount untuk memastikan window sisa (bekas musuh mati) terhapus.

    // Misal: Tadinya ada 5 musuh (0-4). Musuh mati 1, sisa 4 (0-3).
    // Window "enemy_view_4" harus dihapus.

    int maxCheck = enemyCount + 5; // Cek sedikit di atas jumlah sekarang untuk cleanup sisa
    for (int i = enemyCount; i < maxCheck; ++i)
    {
        std::string winID = "enemy_view_" + std::to_string(i);
        if (m_windowSystem->GetTrackedWindow(winID) != nullptr)
        {
            m_windowSystem->RemoveTrackedWindow(winID);
        }
    }
}

void SceneGameBeyond::UpdateItemWindows()
{
    if (!m_itemManager || !m_windowSystem) return;

    const auto& items = m_itemManager->GetItems();

    for (size_t i = 0; i < items.size(); ++i)
    {
        // Buat ID unik berdasarkan index
        std::string winID = "item_view_" + std::to_string(i);
        auto& item = items[i];

        // Cek apakah window untuk item ini sudah ada?
        bool isWindowExists = (m_windowSystem->GetTrackedWindow(winID) != nullptr);

        // KONDISI 1: Item Aktif & Window Belum Ada -> SPAWN WINDOW
        if (item->IsActive() && !isWindowExists)
        {
            m_windowSystem->AddTrackedWindow(
                { winID, "ITEM", 120, 120, 1, { 0.0f, 0.5f, 0.0f } }, // Priority 1 (di atas background)

                // Posisi: Ikuti Item
                [this, i]() -> DirectX::XMFLOAT3 {
                    if (m_itemManager && i < m_itemManager->GetItems().size())
                        return m_itemManager->GetItems()[i]->GetPosition();
                    return { 0, 0, 0 };
                },

                // Ukuran: Tetap
                []() -> DirectX::XMFLOAT2 { return { 120, 120 }; }
            );
        }
        // KONDISI 2: Item Sudah Diambil (Mati) & Window Masih Ada -> DESTROY WINDOW
        else if (!item->IsActive() && isWindowExists)
        {
            // Ini akan memanggil fungsi yang baru kita buat tadi
            m_windowSystem->RemoveTrackedWindow(winID);
        }
    }
}

// =========================================================
// UPDATE LOOP
// =========================================================
void SceneGameBeyond::Update(float elapsedTime)
{
    // 1. Wait for Enter to start
    if (!m_gameStarted)
    {
        if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
        {
            GameWindow* mainWin = Framework::Instance()->GetMainWindow();
            if (mainWin)
            {
                // Trigger efek pecah kaca
                if (m_player)
                {
                    auto pPos = m_player->GetPosition();
                    WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 8);
                }

                // --- UBAH WINDOW MENJADI DEBUG CONSOLE ---
                SDL_Window* sdlWin = mainWin->GetSDLWindow(); // Pastikan GameWindow punya getter ini

                if (sdlWin)
                {
                    // 1. Title & Posisi
                    SDL_SetWindowTitle(sdlWin, "DEBUG CONSOLE");
                    SDL_SetWindowSize(sdlWin, 450, 600);
                    SDL_SetWindowPosition(sdlWin, 20, 20);

                    // 2. [FIX] NYALAKAN BORDER (TITLE BAR) AGAR BISA DIGESER
                    SDL_SetWindowBordered(sdlWin, true);

                    // 3. (Opsional) Biar bisa di-resize manual pakai mouse
                    SDL_SetWindowResizable(sdlWin, true);
                }
            }
            m_gameStarted = true;
            m_shatterTriggered = true;
        }
    }

    // 2. Deferred window initialization
    if (!m_isWindowsInitialized)
    {
        m_startupTimer += elapsedTime;
        if (m_startupTimer > DEFERRED_INIT_TIME)
        {
            InitializeSubWindows();
        }
        return;
    }

    // 3. Update Managers
    WindowShatterManager::Instance().Update(elapsedTime);

    // 4. Game Logic
    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    DirectX::XMFLOAT3 mousePos = GetMouseOnGround(activeCam);

    if (m_player)
    {
        m_player->Update(elapsedTime, activeCam);

        DirectX::XMFLOAT3 pos = m_player->GetPosition();

        // --- SCREEN BOUNDARY COLLISION (DYNAMIC) ---
        // Gunakan variabel member m_screenLimitX dan m_screenLimitZ

        // Clamp X (Kiri Kanan)
        if (pos.x > m_screenLimitX)  pos.x = m_screenLimitX;
        if (pos.x < -m_screenLimitX) pos.x = -m_screenLimitX;

        // Clamp Z (Atas Bawah)
        if (pos.z > m_screenLimitZ)  pos.z = m_screenLimitZ;
        if (pos.z < -m_screenLimitZ) pos.z = -m_screenLimitZ;

        // Anti-Fall
        if (pos.y < 0.0f) pos.y = 0.0f;

        m_player->SetPosition(pos.x, pos.y, pos.z);

        if (m_subCamera) m_subCamera->LookAt(m_player->GetPosition());
    }

    // --- INTEGRASI SHOOT & SHIELD ---
    if (m_blockManager && m_player)
    {
        // Deteksi Input (Menggunakan WinAPI GetKeyState agar responsif untuk hold)
        // VK_LSHIFT = Tameng, VK_SPACE = Tembak
        bool isShielding = (GetKeyState(VK_LSHIFT) & 0x8000);
        bool isShooting = (GetKeyState(VK_SPACE) & 0x8000);

        DirectX::XMFLOAT3 playerPos = m_player->GetPosition();

        // Panggil fungsi logika di BlockManager
        m_blockManager->UpdateShieldLogic(isShielding, mousePos, playerPos, elapsedTime);
        m_blockManager->UpdateShootLogic(isShooting, mousePos, playerPos, elapsedTime);

        // Update biasa
        m_blockManager->Update(elapsedTime, activeCam, m_player.get());

        // Anti-fall untuk blocks (Kode sebelumnya)
        auto& blocks = m_blockManager->GetBlocks();
        for (auto& block : blocks) {
            if (block && block->IsActive()) {
                auto movement = block->GetMovement();
                DirectX::XMFLOAT3 bPos = movement->GetPosition();

                // JIKA JATUH ATAU BERADA DI BAWAH 0
                if (bPos.y < 0.0f)
                {
                    // 1. Reset Posisi ke 0
                    movement->SetPosition({ bPos.x, 0.0f, bPos.z });

                    // 2. HENTIKAN KECEPATAN JATUH (PENTING BIAR GAK JITTER)
                    DirectX::XMFLOAT3 vel = movement->GetVelocity();
                    if (vel.y < 0) {
                        // Nol-kan kecepatan Y, tapi biarkan X dan Z (biar tetap bisa geser)
                        movement->SetVelocity({ vel.x, 0.0f, vel.z });
                    }
                }
            }
        }

        // Debug Spawn manual (tombol R)
        if (Input::Instance().GetKeyboard().IsTriggered('R'))
            m_blockManager->SpawnAllyBlock(m_player.get());
    }
    // --------------------------------

    if (m_boss) m_boss->Update(elapsedTime);

    UpdateProjectileWindows();

    if (m_enemyManager && m_player)
    {
        // Enemy butuh Camera active dan Posisi Player untuk tracking
        Camera* activeCam = CameraController::Instance().GetActiveCamera().get();
        m_enemyManager->Update(elapsedTime, activeCam, m_player->GetPosition());
    
        UpdateEnemyWindows();
    }

    if (m_itemManager)
    {
        UpdateItemWindows(); // Handle Item Windows
    }

    // 5. Update Window System
    if (m_windowSystem)
    {
        m_windowSystem->Update(elapsedTime);
    }

    // 6. Throttled priority enforcement
    m_priorityEnforceTimer += elapsedTime;
    if (m_priorityEnforceTimer >= PRIORITY_ENFORCE_INTERVAL)
    {
        WindowManager::Instance().EnforceWindowPriorities();
        m_priorityEnforceTimer = 0.0f;
    }

    if (m_boss && m_player)
    {
        auto& projectiles = m_boss->GetProjectiles();
        DirectX::XMFLOAT3 pPos = m_player->GetPosition();

        for (auto& proj : projectiles)
        {
            if (!proj.active) continue;

            // Cek Jarak (Hitbox)
            float dx = pPos.x - proj.position.x;
            float dy = pPos.y - proj.position.y;
            float dz = pPos.z - proj.position.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            // Jarak < 1.5 unit berarti kena
            if (distSq < (1.5f * 1.5f))
            {
                // 1. Hapus Projectile (Biar gak kena berkali-kali)
                proj.active = false;

                // 2. Efek Visual: Camera Shake (Guncangan)
                // Ini memberikan impact tanpa menghancurkan layar
                //CameraController::Instance().ApplyShake(0.3f, 0.2f);

                // 3. (Opsional) Log ke Debug Output
                OutputDebugStringA("ouch! player hit.\n");

                // JANGAN PANGGIL WindowShatterManager::Instance().TriggerExplosion(...) DI SINI!
                // Kecuali nyawa player sudah 0 (Game Over).
            }
        }
    }

    HandleDebugInput();
    CameraController::Instance().Update(elapsedTime);

    if (m_itemManager)
    {
        m_itemManager->Update(elapsedTime, activeCam);
    }

    // [BARU] Update Collision
    if (m_collisionManager && m_player)
    {
        // FORCE Stage 3: CollisionManager hanya jalan jika stage == 3
        // (Sesuai logika di CollisionManager.cpp: if (m_player->GetGameStage() != 3) return;)
        if (m_player->GetGameStage() != 3)
        {
            m_player->SetGameStage(3);
        }

        m_collisionManager->Update(elapsedTime);
    }

    if (m_boss && m_blockManager)
    {
        auto& projectiles = m_boss->GetProjectiles();

        // Kita ambil referensi vector blocks biar cepat
        // (Pastikan BlockManager punya getter GetBlocks() yang public)
        const auto& blocks = m_blockManager->GetBlocks();

        // LOOP 1: Untuk setiap Peluru
        for (auto& proj : projectiles)
        {
            if (!proj.active) continue; // Skip peluru mati

            // LOOP 2: Cek terhadap setiap Block
            for (auto& block : blocks)
            {
                // PENTING: Jangan cek block yang sudah hancur!
                // Kalau ini lupa, peluru akan meledak kena "angin" (bekas tempat block)
                if (!block || !block->IsActive()) continue;

                // Ambil posisi
                DirectX::XMFLOAT3 bPos = block->GetMovement()->GetPosition();

                // Hitung Jarak (Squared Distance check lebih cepat daripada sqrt)
                float dx = bPos.x - proj.position.x;
                float dy = bPos.y - proj.position.y;
                float dz = bPos.z - proj.position.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                // Radius Block ~0.8f, Radius File ~0.5f -> Total ~1.3f
                // 1.3 * 1.3 = 1.69f (Kita bulatkan jadi 2.0f biar gampang kena)
                if (distSq < 2.0f)
                {
                    // === TABRAKAN TERJADI ===

                    // 1. Hancurkan Block
                    block->SetActive(false); // Atau panggil fungsi Damage block

                    // 2. Hancurkan Peluru
                    proj.active = false;

                    // 3. Efek Partikel/Suara (Opsional)
                    // WindowShatterManager::Instance().SpawnSmallShard(bPos); 

                    // 4. [SANGAT PENTING] BREAK INNER LOOP
                    // Karena peluru ini SUDAH MATI, dia tidak boleh mengecek block lain lagi.
                    // Jika tidak di-break, peluru mati ini bisa membunuh block lain di frame yang sama.
                    break;
                }
            }
        }
    }
}

static void DrawTransformControl(const char* label, DirectX::XMFLOAT3* pos)
{
    if (ImGui::TreeNode(label))
    {
        if (pos) ImGui::DragFloat3("Position", &pos->x, 0.1f);
        ImGui::TreePop();
    }
}

// ======================================================== =
// DRAW GUI - MAIN ENTRY
// =========================================================
void SceneGameBeyond::DrawGUI()
{
    // Setup Window Style
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("BEYOND INSPECTOR", nullptr))
    {
        if (ImGui::BeginTabBar("BeyondTabs"))
        {
            if (ImGui::BeginTabItem("General")) { DrawTabGeneral();      ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Objects")) { DrawTabObjects();      ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Window System")) { DrawTabWindowSystem(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Camera")) { DrawTabCamera();       ImGui::EndTabItem(); }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// =========================================================
// GUI HELPER FUNCTIONS
// =========================================================

void SceneGameBeyond::DrawTabGeneral()
{
    ImGui::Spacing();

    // --- SECTION 1: GAME STATE ---
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "GAME STATE");
    ImGui::Checkbox("Game Started", &m_gameStarted);
    ImGui::Checkbox("Shatter Triggered", &m_shatterTriggered);
    ImGui::Separator();

    // --- SECTION 2: BOUNDARY ---
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PLAYER BOUNDARY (1920x1080)");
    ImGui::TextDisabled("Ratio: 1 Unit = 40 Pixels");

    ImGui::Indent();
    ImGui::DragFloat("Limit X (Width)", &m_screenLimitX, 0.1f, 10.0f, 50.0f);
    ImGui::DragFloat("Limit Z (Height)", &m_screenLimitZ, 0.1f, 5.0f, 30.0f);
    ImGui::Unindent();
    ImGui::Separator();

    // --- SECTION 3: SHATTER FX ---
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WINDOW SHATTER FX");

    // Info Stats
    auto& shards = WindowShatterManager::Instance().GetShatters();
    ImGui::Text("Active Shards: %d", (int)shards.size());

    // Controls
    if (ImGui::Button("TRIGGER EXPLOSION", ImVec2(-1, 40)))
    {
        if (m_player) {
            auto pPos = m_player->GetPosition();
            WindowShatterManager::Instance().TriggerExplosion({ pPos.x, pPos.z }, 12);
            m_shatterTriggered = true;
        }
    }

    if (ImGui::Button("Clear All Shards", ImVec2(-1, 0))) {
        WindowShatterManager::Instance().Clear();
    }
}

void SceneGameBeyond::DrawTabObjects()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "SCENE HIERARCHY");
    ImGui::Separator();

    // --- 1. PLAYER ---
    if (m_player)
    {
        if (ImGui::TreeNode("Player"))
        {
            XMFLOAT3 pos = m_player->GetPosition();
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                m_player->SetPosition(pos.x, pos.y, pos.z);
            }
            ImGui::TreePop();
        }
    }

    // --- 2. BOSS ---
    if (m_boss)
    {
        if (ImGui::TreeNode("Boss Controller"))
        {
            m_boss->DrawDebugGUI(); // Delegasi ke fungsi internal Boss
            ImGui::TreePop();
        }
    }

    ImGui::Separator();

    // --- 3. ENEMY MANAGER ---
    if (m_enemyManager)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "ENEMY MANAGER");

        size_t count = m_enemyManager->GetEnemies().size();
        ImGui::Text("Active Enemies: %d", (int)count);

        // A. GLOBAL SETTINGS (Collapsing Header biar rapi)
        if (ImGui::CollapsingHeader("Global Window Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::Text("Camera Offset:");
            ImGui::DragFloat3("##TrackOffset", &m_enemyTrackOffset.x, 0.05f);

            ImGui::Text("Size Offset (Base 250):");
            ImGui::DragFloat2("##SizeOffset", &m_enemySizeOffset.x, 1.0f);

            if (ImGui::Button("Reset All Offsets")) {
                m_enemyTrackOffset = { 0.0f, 0.0f, 0.0f };
                m_enemySizeOffset = { 0.0f, 0.0f };
            }
            ImGui::Unindent();
        }

        // B. INDIVIDUAL LIST
        if (count > 0)
        {
            if (ImGui::TreeNode("Individual Enemy Control"))
            {
                auto& enemies = m_enemyManager->GetEnemies();
                for (int i = 0; i < count; ++i)
                {
                    auto& enemy = enemies[i];
                    if (!enemy) continue;

                    ImGui::PushID(i); // Wajib agar ID unik

                    std::string label = "Enemy " + std::to_string(i);
                    if (ImGui::TreeNode(label.c_str()))
                    {
                        DirectX::XMFLOAT3 pos = enemy->GetPosition();
                        if (ImGui::DragFloat3("Pos", &pos.x, 0.1f)) enemy->SetPosition(pos);

                        DirectX::XMFLOAT3 rot = enemy->GetRotation();
                        if (ImGui::DragFloat3("Rot", &rot.x, 1.0f)) enemy->SetRotation(rot);

                        ImGui::TextDisabled("Type ID: %d", (int)enemy->GetType());
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
        else
        {
            ImGui::TextDisabled("No enemies. Spawn one via Boss!");
        }

        // --- ANTENNA WINDOW SETTINGS ---
        if (ImGui::TreeNode("Antenna Window Settings"))
        {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Tracking Offset (World Units)");
            ImGui::DragFloat3("Pos Offset", &m_antennaTrackOffset.x, 0.1f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Size Adjustment (Pixels)");
            ImGui::DragFloat2("Size (+/-)", &m_antennaSizeOffset.x, 1.0f);

            // Tombol Reset biar gampang kalau kekecilan/kejauhan
            if (ImGui::Button("Reset Antenna Config"))
            {
                m_antennaTrackOffset = { 0.0f, 4.0f, 0.0f };
                m_antennaSizeOffset = { 0.0f, 0.0f };
            }

            ImGui::TreePop();
        }
    }
}

void SceneGameBeyond::DrawTabWindowSystem()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TRACKED WINDOWS SYSTEM");
    ImGui::Separator();

    if (m_windowSystem)
    {
        ImGui::Text("Unified FOV: %.1f", FIELD_OF_VIEW);

        // Bisa ditambah info lain, misal jumlah total window di manager
        // ImGui::Text("Total Windows: %d", ...);

        ImGui::Spacing();
        ImGui::TextWrapped("System is active. New windows will spawn automatically based on logic.");
    }
    else
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "CRITICAL: Window System is NULL");
    }
}

void SceneGameBeyond::DrawTabCamera()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MAIN CAMERA DEBUG");
    ImGui::Separator();

    if (m_mainCamera)
    {
        XMFLOAT3 pos = m_mainCamera->GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
            m_mainCamera->SetPosition(pos);
        }

        ImGui::Spacing();
        if (ImGui::Button("Reset Camera Pos")) {
            m_mainCamera->SetPosition(0, 5, 0); // Atur default sesuai kebutuhan
        }
    }
}

// =========================================================
// RENDER
// =========================================================
void SceneGameBeyond::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    if (!m_gameStarted && targetCam == m_mainCamera.get())
    {
        D3D11_VIEWPORT vp;
        UINT num = 1;
        dc->RSGetViewports(&num, &vp);
        m_primitive2D->Rect(0.0f, 0.0f, vp.Width, vp.Height, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        m_primitive2D->Render(dc);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    }

    RenderScene(elapsedTime, targetCam);

    if (targetCam == m_mainCamera.get())
    {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }
}

void SceneGameBeyond::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };

    // --- 1. RENDER PLAYER (CULLING) ---
    if (m_player)
    {
        XMFLOAT3 pPos = m_player->GetPosition();
        // Radius player kira-kira 1.0f
        if (camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f))
        {
            m_player->Render(modelRenderer);
        }
    }

    // --- 2. RENDER BOSS (CULLING INTERNAL) ---
    if (m_boss && m_gameStarted)
    {
        // Boss sudah punya logic culling di dalamnya (Langkah 2)
        m_boss->Render(modelRenderer, camera);
    }

    // --- 3. RENDER ENEMY (CULLING) ---
    if (m_enemyManager && m_gameStarted)
    {
        // Panggil fungsi render manager, tapi kirim camera untuk culling
        m_enemyManager->Render(modelRenderer, camera);
    }

    // --- 4. RENDER BLOCKS (CULLING) ---
    if (m_blockManager && m_gameStarted)
    {
        for (const auto& block : m_blockManager->GetBlocks())
        {
            if (block->IsActive())
            {
                // Ambil posisi dari movement component
                XMFLOAT3 bPos = block->GetMovement()->GetPosition();

                // Block kecil, radius 0.5f cukup
                if (camera->CheckSphere(bPos.x, bPos.y, bPos.z, 0.8f))
                {
                    block->Render(modelRenderer, m_blockManager->globalBlockColor);
                }
            }
        }
    }

    if (m_itemManager)
    {
        // ItemManager butuh ModelRenderer, ambil dari Graphics Instance
        auto modelRenderer = Graphics::Instance().GetModelRenderer();
        m_itemManager->Render(modelRenderer);
    }

    modelRenderer->Render(rc);

    // Virtual Shatters (Overlay 2D)
    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    const auto& shatters = WindowShatterManager::Instance().GetShatters();
    for (const auto& shatter : shatters)
    {
        if (!shatter) continue;

        if (!shatter->IsNativeWindow())
        {
            DirectX::XMFLOAT3 worldPos = shatter->GetVirtualWorldPos();
            DirectX::XMFLOAT2 size = shatter->GetSize();

            float screenX, screenY;
            if (m_windowSystem)
            {
                m_windowSystem->WorldToScreenPos(worldPos, screenX, screenY);

                m_primitive2D->Rect(
                    screenX, screenY,
                    size.x, size.y,
                    size.x * 0.5f, size.y * 0.5f,
                    0.0f,
                    1.0f, 1.0f, 1.0f, 1.0f
                );
            }
        }
    }

    m_primitive2D->Render(dc);

    dc->OMSetDepthStencilState(Graphics::Instance().GetRenderState()->GetDepthStencilState(DepthState::TestAndWrite), 0);
}

void SceneGameBeyond::OnResize(int width, int height)
{
    if (m_mainCamera) m_mainCamera->SetAspectRatio((float)width / max(1, height));
}

void SceneGameBeyond::HandleDebugInput()
{
    if (Input::Instance().GetKeyboard().IsTriggered('N'))
    {
        GameWindow* addWin = WindowManager::Instance().CreateGameWindow("Debug Cam", 300, 300);
        auto addCam = std::make_shared<Camera>();
        addCam->SetPerspectiveFov(XMConvertToRadians(60), 1.0f, 0.1f, 1000.0f);
        addWin->SetCamera(addCam.get());
        m_additionalCameras.push_back(addCam);
    }
}

// GANTI FUNGSI INI DI SceneGameBeyond.cpp
DirectX::XMFLOAT3 SceneGameBeyond::GetMouseOnGround(Camera* camera)
{
    // 1. Ambil Posisi Mouse Global (Koordinat Layar Monitor)
    POINT pt;
    if (!GetCursorPos(&pt)) return { 0,0,0 }; // WinAPI function

    // 2. Ambil Ukuran Layar Monitor Utama (Resolusi Asli)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 3. Hitung Titik Tengah Layar (Center of World 0,0,0)
    float centerX = screenW / 2.0f;
    float centerY = screenH / 2.0f;

    // 4. Hitung Jarak Mouse dari Tengah Layar (dalam Pixel)
    float dx = (float)pt.x - centerX;
    float dy = (float)pt.y - centerY;

    // 5. Konversi Pixel ke World Unit
    // Kita gunakan Ratio yang sama dengan WindowTrackingSystem (40.0f)
    // Rumus: Jarak Pixel / Ratio = Jarak World

    float worldX = dx / PIXEL_TO_UNIT_RATIO;

    // Note: Screen Y positif ke bawah, tapi World Z positif ke atas (tergantung kamera).
    // Biasanya untuk top-down view desktop: Y layar turun = Z world mundur.
    float worldZ = -dy / PIXEL_TO_UNIT_RATIO;

    return DirectX::XMFLOAT3(worldX, 0.0f, worldZ);
}

void SceneGameBeyond::UpdateProjectileWindows()
{
    if (!m_boss || !m_windowSystem) return;

    const auto& projectiles = m_boss->GetProjectiles();
    int windowFrequency = 4;

    // =========================================================
    // TAHAP 1: DATA GATHERING (MARK)
    // Catat semua ID Projectile yang "Berhak" punya window saat ini.
    // =========================================================
    std::unordered_set<int> activeProjectileIDs;
    for (const auto& p : projectiles)
    {
        if (p.active && (p.id % windowFrequency == 0))
        {
            activeProjectileIDs.insert(p.id);
        }
    }

    // =========================================================
    // TAHAP 2: CLEANUP ORPHANS (SWEEP)
    // Cek semua window yang ada. Jika ID-nya tidak ada di daftar aktif, HAPUS.
    // =========================================================

    // Kita butuh list sementara agar tidak merusak iterator saat looping
    std::vector<std::string> windowsToRemove;
    const auto& currentWindows = m_windowSystem->GetWindows();

    for (const auto& win : currentWindows)
    {
        // Cek apakah ini window milik projectile (prefix "file_proj_")
        if (win->name.find("file_proj_") == 0)
        {
            // Ambil ID dari nama string. "file_proj_" panjangnya 10 karakter.
            // Contoh: "file_proj_105" -> diambil "105"
            try {
                std::string idStr = win->name.substr(10);
                int id = std::stoi(idStr);

                // Jika ID ini TIDAK ada di daftar projectile aktif -> Masukkan ke tong sampah
                if (activeProjectileIDs.find(id) == activeProjectileIDs.end())
                {
                    windowsToRemove.push_back(win->name);
                }
            }
            catch (...) { /* Safety jika parsing gagal */ }
        }
    }

    // Eksekusi pembersihan (Release ke Pool)
    for (const auto& name : windowsToRemove)
    {
        m_windowSystem->ReleasePooledWindow(name);
    }

    // =========================================================
    // TAHAP 3: SPAWN NEW WINDOWS
    // Buat window untuk projectile baru yang belum punya window
    // =========================================================
    for (const auto& p : projectiles)
    {
        if (p.active && (p.id % windowFrequency == 0))
        {
            std::string winName = "file_proj_" + std::to_string(p.id);

            // Jika belum ada window-nya, bikin baru (ambil dari pool)
            if (m_windowSystem->GetTrackedWindow(winName) == nullptr)
            {
                int targetID = p.id;
                Boss* targetBoss = m_boss.get();

                m_windowSystem->AddPooledTrackedWindow(
                    {
                        winName,
                        "DOWNLOADING...",
                        120,
                        120,
                        10,
                        { 0.0f, 0.0f, 0.0f },
                        20.0f // FPS Limit
                    },
                    [targetBoss, targetID]() -> DirectX::XMFLOAT3 {
                        DirectX::XMFLOAT3 pos = { 0,0,0 };
                        if (targetBoss->GetProjectileData(targetID, pos)) return pos;
                        return { 0, -1000, 0 };
                    }
                );
            }
        }
    }
}