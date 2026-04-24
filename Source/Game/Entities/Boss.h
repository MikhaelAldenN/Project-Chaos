#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "System/ModelRenderer.h"
#include "Camera.h"
#include "BossTerminal.h"
#include "TerminalMonitor1.h"
#include "StateBoss.h"
#include "EnemyManager.h"
#include "System/Sprite.h"
#include "Enemy.h"

class Player;

// =========================================================
// DATA STRUCTURES
// =========================================================

struct BossPartConfig
{
    std::string name;
    std::string modelPath;
    DirectX::XMFLOAT3 position = { 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 rotation = { 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 scale = { 1.f, 1.f, 1.f };
    bool useFloating = true;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.f, 1.f, 0.f };
    float cullRadius = 2.0f;
    DirectX::XMFLOAT3 cullOffset = { 0.f, 0.f, 0.f };
};

struct BossPart
{
    std::string name;
    std::shared_ptr<Model> model;

    // Transform Data
    DirectX::XMFLOAT3 position = { 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 rotation = { 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 scale = { 1.f, 1.f, 1.f };

    // Visual State (Calculated per frame)
    DirectX::XMFLOAT3 visualPosition = { 0.f, 0.f, 0.f };

    // Animation Settings
    bool useFloating = true;
    float timer = 0.0f;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.f, 1.f, 0.f };

    // Culling
    float cullRadius = 2.0f;
    DirectX::XMFLOAT3 cullOffset = { 0.f, 0.f, 0.f };

    void Update(float dt);
    void Render(ModelRenderer* renderer);
};

enum class BgAnimState { HIDDEN, ENTERING, ACTIVE, EXITING };

struct FileProjectile
{
    int id = -1;
    bool active = false;
    DirectX::XMFLOAT3 position = { 0,0,0 };
    DirectX::XMFLOAT3 rotation = { 0,0,0 };
    DirectX::XMFLOAT3 targetPos = { 0,0,0 };
    float speed = 20.0f;
};

// ==========================================
// WIRE ATTACK STRUCTURES
// ==========================================
enum class WireState {
    INACTIVE,
    TRAVELING,  // Sedang terbang ke target
    WARNING,    // Nancep tapi belum nyetrum (Delay 2 detik)
    ACTIVE,     // Nyetrum (Damage Player)
    FADING      // Animasi hilang
};

struct ElectricWire
{
    int id = -1;
    WireState state = WireState::INACTIVE;

    // Transform
    DirectX::XMFLOAT3 position = { 0,0,0 }; // Posisi ujung kabel
    DirectX::XMFLOAT3 startPos = { 0,0,0 }; // Posisi asal (Boss)
    DirectX::XMFLOAT3 rotation = { 0,0,0 };
    DirectX::XMFLOAT3 direction = { 0,0,1 };

    // Logic
    float timer = 0.0f;
    float speed = 25.0f;
    int modelIndex = 0; // 0 = Wire1, 1 = Wire2

    // Collision Params
    float length = 10.0f; // Panjang kabel visual
    float radius = 1.5f;  // Lebar area setrum
};

// =========================================================
// BOSS CONTROLLER
// =========================================================

class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void DrawDebugGUI();

    // --- Part System ---
    BossPart* GetPart(const std::string& name);
    bool HasPart(const std::string& name) const;

    // Shortcuts for Window Tracking
    DirectX::XMFLOAT3 GetPartVisualPos(const std::string& name) const;
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return GetPartVisualPos("monitor1"); }
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return GetPartVisualPos("cpu"); }
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return GetPartVisualPos("monitor2"); }
    DirectX::XMFLOAT3 GetMonitor3VisualPos() const { return GetPartVisualPos("monitor3"); }
    DirectX::XMFLOAT3 GetAntennaVisualPos() const { return GetPartVisualPos("antenna"); }

    // --- State & Logic ---
    void ChangeState(class BossState* newState);
    BossStateMachine* GetStateMachine() { return &m_stateMachine; }

    // Triggers
    void TriggerIdle();
    void TriggerSpawnEnemy();
    void TriggerLockPlayer();
    void TriggerSpawnPentagon();
    void TriggerDownloadAttack();      // [BARU] Trigger untuk Download Attack State
    void TriggerWireAttackState();     // [BARU] Trigger untuk Wire Attack State (beda dari TriggerWireAttack mechanic)

    // --- Dependencies ---
    void SetPlayer(Player* p) { m_player = p; }
    Player* GetPlayer() const { return m_player; }
    void SetEnemyManager(class EnemyManager* em) { m_enemyManager = em; }
    class EnemyManager* GetEnemyManager() const { return m_enemyManager; }

    // --- Terminals ---
    void AddTerminalLog(const std::string& msg);
    TerminalMonitor1* GetMonitor1() { return &m_terminal1; }

    // --- Projectile System ---
    std::vector<FileProjectile>& GetProjectiles() { return m_fileProjectiles; }
    void SpawnFileProjectile(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& target);
    void ClearProjectiles();
    bool GetProjectileData(int id, DirectX::XMFLOAT3& outPos) const;

    // --- Configuration (Exposed for State/GUI) ---
    struct AntennaConfig {
        DirectX::XMFLOAT3 hiddenPos = { 30.5f, 0.0f, -8.7f };
        DirectX::XMFLOAT3 showPos = { 21.5f, 0.0f, -8.7f };
        DirectX::XMFLOAT3 scale = { 8.0f, 8.0f, 8.0f };
        DirectX::XMFLOAT3 rotation = { 240.0f, 132.0f, 52.0f };
        DirectX::XMFLOAT3 spawnSrc = { -19.0f, 0.0f, 1.7f };
    } m_antennaConfig;

    struct PentagonConfig {
        float scale = 150.0f;
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
    } m_pentagonConfig;

    struct WireConfig {
        DirectX::XMFLOAT3 scale = { 100.0f, 100.0f, 100.0f }; // Untuk Scale (Besar/Kecil)
        DirectX::XMFLOAT3 pivotOffset = { 16.0f, -0.5f, 0.0f };
        float speed = 25.0f;       // Kecepatan terbang
        float targetSpread = 1.0f; // Jarak lebar tembakan (kiri/kanan dari player)
        float hitRadius = 1.5f;    // Besar area setrum (Collision)
    } m_wireConfig;

    bool m_debugShowAntenna = false;

    void TriggerWireAttack();

    // Fungsi render khusus wire
    void RenderWires(ModelRenderer* renderer);

    // ==========================================
    // CONCURRENT SPAWN SYSTEM
    // Spawn minion tetap jalan meskipun state lain
    // (LockPlayer / DownloadAttack / WireAttack) sedang aktif.
    // ==========================================
    struct ConcurrentSpawnConfig
    {
        bool enabled = true;                // Master switch
        float interval = 3.5f;              // Jeda antar spawn (detik)
        float activateDelay = 1.5f;         // Delay dari awal state sebelum pertama spawn
        float safeDistance = 8.0f;           // Min jarak ke player (anti instant-kill)
    } m_concurrentSpawnConfig;

    bool IsConcurrentSpawnEligibleState() const;


private:
    void InitializeParts();
    void UpdateBackgroundAnim(float dt);
    void RenderScreens(ModelRenderer* renderer);
    void RenderProjectiles(ModelRenderer* renderer, Camera* camera);
    // Fungsi untuk State Machine atau Boss Logic memanggil serangan

private:
    // Core Components
    std::vector<std::unique_ptr<BossPart>> m_parts;
    std::unordered_map<std::string, BossPart*> m_partMap;
    BossStateMachine m_stateMachine;

    // External Refs
    Player* m_player = nullptr;
    EnemyManager* m_enemyManager = nullptr;

    // Sub-Systems
    BossTerminal m_terminal;      // Logs
    TerminalMonitor1 m_terminal1; // Commands

    // Visuals: Screens
    std::shared_ptr<Model> m_screenQuad;
    std::shared_ptr<Model> m_screenQuad1;

    // Visuals: Background Chain
    std::unique_ptr<Sprite> m_bgChainSprite;
    BgAnimState m_bgState = BgAnimState::HIDDEN;
    float m_bgAlpha = 0.0f;
    float m_bgTimer = 0.0f;
    bool m_debugForceBG = false;

    // Visuals: Projectiles
    std::shared_ptr<Model> m_fileModel;
    std::vector<FileProjectile> m_fileProjectiles;
    int m_projectileIdCounter = 0;

    // Screen Transform Configs
    struct ScreenTransform {
        DirectX::XMFLOAT3 offset;
        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT3 scale;
    };
    ScreenTransform m_screen2Config = { {0.0f, 0.002f, 0.018f}, {81.7f, 0.0f, 0.0f}, {3.2f, 3.2f, 3.2f} };
    ScreenTransform m_screen1Config = { {-0.02f, 0.195f, -0.34f}, {90.0f, 180.0f, 0.0f}, {46.0f, 46.0f, 46.0f} };

    std::vector<std::shared_ptr<Model::Material>> m_materialCache;

    void UpdateWires(float dt);
    void SpawnSingleWire(const DirectX::XMFLOAT3& targetPos);

    // Concurrent spawn
    float m_concurrentSpawnTimer = 0.0f;

    // Resources
    std::shared_ptr<Model> m_wireModel1;
    std::shared_ptr<Model> m_wireModel2;

    // Object Pool
    static const int MAX_WIRES = 10;
    std::vector<ElectricWire> m_wires;
};