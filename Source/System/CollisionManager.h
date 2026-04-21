#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <functional>
#include <vector>
#include "System/AudioManager.h"
#include "System/Collision.h"
#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Boss.h"

class ItemManager;
class Boss;

class CollisionManager
{
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    // OVERLOAD 1: Untuk SceneGameBreaker (Tidak butuh Boss)
    void Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im);

    // OVERLOAD 2: Untuk SceneGameBeyond (Butuh Boss)
    void Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im, Boss* boss);

    void Update(float elapsedTime);
    void SetOnCheckpointReachCallback(std::function<void(DirectX::XMFLOAT3)> callback) { m_onCheckpointReachCallback = callback; }
    void SetOnLevelCompleteCallback(std::function<void()> callback) { m_onLevelCompleteCallback = callback; }
    void SetOnPlayerDeathCallback(std::function<void()> callback) { m_onPlayerDeathCallback = callback; }
    void SetOnPlayerHitCallback(std::function<void()> callback) { m_onPlayerHitCallback = callback; }

private:
    void CheckPlayerVsCheckpointLines();
    void CheckPlayerVsEnemies();
    void CheckPlayerVsItems();
    void CheckPlayerVsTriggerLines();
    void CheckPlayerVsVoidLines();
    void CheckStageCollision();
    void CheckEnemyProjectilesFull(float elapsedTime);
    void CheckBossFilesVsPlayer();

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;
    Boss* m_boss = nullptr;

    EnemyManager* m_enemyManager = nullptr;
    ItemManager* m_itemManager = nullptr;

    std::function<void(DirectX::XMFLOAT3)> m_onCheckpointReachCallback;
    std::function<void()> m_onLevelCompleteCallback = nullptr;
    std::function<void()> m_onPlayerDeathCallback;
    std::function<void()> m_onPlayerHitCallback = nullptr;
};