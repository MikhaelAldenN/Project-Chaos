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

class Player;
class BitmapFont;

// =========================================================
// STRUCTS
// =========================================================

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

    float cullRadius = 2.0f;
    DirectX::XMFLOAT3 cullOffset = { 0.0f, 0.0f, 0.0f };
};

struct BossPart
{
    std::string name;
    std::shared_ptr<Model> model;

    // Transform
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    // Animation State
    DirectX::XMFLOAT3 visualPosition = { 0.0f, 0.0f, 0.0f };
    bool useFloating = true;
    float timer = 0.0f;
    float floatSpeed = 2.0f;
    float floatIntensity = 0.2f;
    DirectX::XMFLOAT3 floatAxis = { 0.0f, 1.0f, 0.0f };

    void Update(float dt);
    void Render(ModelRenderer* renderer);
    float cullRadius = 2.0f;
    DirectX::XMFLOAT3 cullOffset = { 0.0f, 0.0f, 0.0f };

};

// =========================================================
// BOSS CLASS (MAIN CONTROLLER)
// =========================================================

class Boss
{
public:
    Boss();
    ~Boss();

    // --- Core Lifecycle ---
    void Update(float dt);
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void DrawDebugGUI();

    // --- Part Management ---
    bool AddPart(const BossPartConfig& config);
    BossPart* GetPart(const std::string& name);
    DirectX::XMFLOAT3 GetPartVisualPos(const std::string& name) const;
    bool HasPart(const std::string& name) const;
    std::vector<std::string> GetPartNames() const;

    // --- Legacy Part Shortcuts ---
    DirectX::XMFLOAT3 GetMonitorVisualPos() const { return GetPartVisualPos("monitor1"); }
    DirectX::XMFLOAT3 GetCPUVisualPos() const { return GetPartVisualPos("cpu"); }
    DirectX::XMFLOAT3 GetMonitor2VisualPos() const { return GetPartVisualPos("monitor2"); }
    DirectX::XMFLOAT3 GetMonitor3VisualPos() const { return GetPartVisualPos("monitor3"); }

    // --- AI & State Management ---
    void ChangeState(class BossState* newState);
    BossStateMachine* GetStateMachine() { return &m_stateMachine; }

    // AI Commands (Triggers)
    void TriggerIdle();
    void TriggerSpawnEnemy();
    void TriggerLockPlayer();

    // --- External Dependencies ---
    void SetPlayer(Player* player) { m_player = player; }
    Player* GetPlayer() const { return m_player; }

    void SetEnemyManager(class EnemyManager* em) { m_enemyManager = em; }
    class EnemyManager* GetEnemyManager() const { return m_enemyManager; }

    // --- Terminal Access ---
    void AddTerminalLog(const std::string& msg); // For Monitor 2 (Log System)
    TerminalMonitor1* GetMonitor1() { return &m_terminal1; } // For Monitor 1 (Command System)

private:
    void InitializeDefaultParts();
    void CreateScreenQuad(); // Helper to create screen geometry

private:
    // --- Components ---
    std::vector<std::unique_ptr<BossPart>> m_parts;
    std::unordered_map<std::string, BossPart*> m_partLookup;
    BossStateMachine m_stateMachine;

    // --- External References ---
    Player* m_player = nullptr;
    class EnemyManager* m_enemyManager = nullptr;

    // --- Terminal Systems ---
    BossTerminal m_terminal;      // Monitor 2 (Logs)
    TerminalMonitor1 m_terminal1; // Monitor 1 (Commands/Cursor)

    // --- Screen Geometry (Monitor Overlays) ---
    // We use separate models for screens to support different textures
    std::shared_ptr<Model> m_screenQuad;  // For Monitor 2
    std::shared_ptr<Model> m_screenQuad1; // For Monitor 1

    // --- Screen Transforms (Configurable via ImGui) ---

    // Monitor 2 Screen (Relative Transform)
    DirectX::XMFLOAT3 m_screenOffset = { 0.0f, 0.002f, 0.0174f };
    DirectX::XMFLOAT3 m_screenRotation = { 81.7f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_screenScale = { 3.2f, 3.2f, 3.2f };

    // Monitor 1 Screen (Relative Transform)
    DirectX::XMFLOAT3 m_screen1Offset = { -0.02f, 0.195f, -0.340f };
    DirectX::XMFLOAT3 m_screen1Rotation = { 90.0f, 180.0f, 0.0f };
    DirectX::XMFLOAT3 m_screen1Scale = { 46.0f, 46.0f, 46.0f };

    std::unique_ptr<Sprite> m_bgChainSprite;

    DirectX::XMFLOAT3 m_bgChainPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_bgChainRotation = { 90.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_bgChainSize = { 48.0f, 26.0f };

    bool m_debugForceBG = false;
};