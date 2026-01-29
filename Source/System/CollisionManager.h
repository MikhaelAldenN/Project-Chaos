#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <functional>
#include <vector>
#include "System/Collision.h"
#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "Block.h"
#include "BlockManager.h"
#include "EnemyManager.h"
#include "ItemManager.h"

class ItemManager;

class CollisionManager
{
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    void Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em, ItemManager* im);
    void Update(float elapsedTime);
    void SetOnPlayerHitCallback(std::function<void()> callback) { m_onPlayerHitCallback = callback; }

private:
    void UpdateBlockStackFlags();
    void CheckBlockVsBlocks();
    void CheckBlockVsEnemies();
    void CheckBlockVsItems();
    void CheckBlockVsStage();
    void CheckBlockVsVoidLines();
    void CheckPlayerVsBlocks();
    void CheckPlayerVsEnemies();
    void CheckPlayerVsItems();
    void CheckPlayerVsVoidLines();
    void CheckStageCollision();
    void CheckEnemyProjectilesFull(float elapsedTime);

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;
    BlockManager* m_blockManager = nullptr;
    EnemyManager* m_enemyManager = nullptr;
    ItemManager* m_itemManager = nullptr;
    SpatialHashGrid m_blockGrid;

    std::function<void()> m_onPlayerHitCallback = nullptr;
};