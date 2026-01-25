#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <vector>
#include "System/Collision.h"
#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "Block.h"
#include "BlockManager.h"
#include "EnemyManager.h"

class CollisionManager
{
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    void Initialize(Player* p, Stage* s, BlockManager* bm, EnemyManager* em);
    void Update(float elapsedTime);

private:
    void CheckBlockVsBlocks();
    void UpdateBlockStackFlags();
    void CheckBlockVsStage();
    void CheckBlockVsVoidLines();
    void CheckPlayerVsBlocks();
    void CheckStageCollision();
    void CheckEnemyProjectilesFull(float elapsedTime);

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;
    BlockManager* m_blockManager = nullptr;
    EnemyManager* m_enemyManager = nullptr;
    SpatialHashGrid m_blockGrid;
};