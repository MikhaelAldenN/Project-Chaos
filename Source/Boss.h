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

class Player;
// Forward declaration
class BitmapFont;

// =========================================================
// BOSS PART - Single Component
// =========================================================
struct BossPart
{
    std::string name;
    std::shared_ptr<Model> model;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    DirectX::XMFLOAT3 visualPosition = { 0.0f, 0.0f, 0.0f };

    bool useFloating = true;
    float timer = 0.0f;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };

    void Update(float dt);
    void Render(ModelRenderer* renderer);
};

struct BossPartConfig
{
    std::string name;
    std::string modelPath;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    bool useFloating = true;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };
};

// =========================================================
// BOSS CLASS - Main Manager
// =========================================================
class Boss
{
public:
    Boss();
    ~Boss();

    void Update(float dt);
    // [UPDATE] Render sekarang butuh Camera pointer untuk teks 3D
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void DrawDebugGUI();

    bool AddPart(const BossPartConfig& config);
    BossPart* GetPart(const std::string& name);
    DirectX::XMFLOAT3 GetPartVisualPos(const std::string& name) const;
    bool HasPart(const std::string& name) const;
    std::vector<std::string> GetPartNames() const;

    // --- LEGACY GETTERS ---
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return GetPartVisualPos("monitor1"); }
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return GetPartVisualPos("cpu"); }
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return GetPartVisualPos("monitor2"); }
    DirectX::XMFLOAT3 GetMonitor3VisualPos() const { return GetPartVisualPos("monitor3"); }

    // [BARU] Public access untuk nambah log dari luar (misal saat player kena hit)
    void AddTerminalLog(const std::string& msg);

    BossPart* GetMonitorPart() { return GetPart("monitor1"); }
    BossPart* GetCPUPart() { return GetPart("cpu"); }
    BossPart* GetMonitor2Part() { return GetPart("monitor2"); }
    BossPart* GetMonitor3Part() { return GetPart("monitor3"); }
    BossPart* GetCable1() { return GetPart("cable1"); }

    // [TAMBAHKAN INI] Setter/Getter EnemyManager (PENTING untuk SpawnEnemyState)
    void SetEnemyManager(class EnemyManager* em) { m_enemyManager = em; }
    class EnemyManager* GetEnemyManager() const { return m_enemyManager; }

    // [TAMBAHKAN INI] Helper ganti state dari luar
    void ChangeState(class BossState* newState);

    // [TAMBAHKAN INI] Getter StateMachine (Ini yang bikin error C2039)
    BossStateMachine* GetStateMachine() { return &m_stateMachine; }

    void SetPlayer(Player* player) { m_player = player; }
    Player* GetPlayer() const { return m_player; }

    TerminalMonitor1* GetMonitor1() { return &m_terminal1; }

    void TriggerIdle();
    void TriggerSpawnEnemy();
    void TriggerLockPlayer();

private:
    void InitializeDefaultParts();

private:
    std::vector<std::unique_ptr<BossPart>> m_parts;
    std::unordered_map<std::string, BossPart*> m_partLookup;

    // [BARU] Sistem Terminal 3D
    BossTerminal m_terminal;
    TerminalMonitor1 m_terminal1; // Terminal untuk monitor 1


    std::shared_ptr<Model> m_screenQuad1; // Model layar untuk monitor 1

    std::shared_ptr<Model> m_screenQuad; // Model kotak tipis untuk layar
    void CreateScreenQuad(); // Fungsi bikin kotak manual

    DirectX::XMFLOAT3 m_screenOffset = { 0.0f, 0.002f, 0.0174f };
    DirectX::XMFLOAT3 m_screenRotation = { 81.7f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_screenScale = { 3.2f, 3.2f, 3.2f };
    // Transform untuk monitor 1 screen
    // Monitor 1 scale = 10, jadi butuh screen scale lebih kecil dari monitor 2 (yang scale 100)
    DirectX::XMFLOAT3 m_screen1Offset = { -0.02f, 0.195f, -0.340f };
    DirectX::XMFLOAT3 m_screen1Rotation =  { 90.0f, 180.0f, 0.0f };
    DirectX::XMFLOAT3 m_screen1Scale = { 46.0f, 46.0f, 46.0f };


    BossStateMachine m_stateMachine;
    class EnemyManager* m_enemyManager = nullptr;

    Player* m_player = nullptr;
};