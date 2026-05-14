#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <functional>
#include <vector>
#include "System/AudioManager.h"
#include "System/Collision.h"
#include "System/Input.h"
#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "StateMachine.h"
#include "PlayerStates.h"
#include "Boss.h"
#include "NaviAlly.h"

class ItemManager;
class Boss;
class NaviAlly;
class NaviBoss;

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
    [[nodiscard]] Enemy* GetTargetInSlashCone(const DirectX::XMFLOAT3& playerPos, const DirectX::XMFLOAT3& aimDir, float reach, float minDotProduct) const;
    bool GetParryableProjectile(const DirectX::XMFLOAT3& playerPos, float threshold, class Bullet** outBullet, Enemy** outNearestEnemy);
    void SetNavi(NaviAlly* navi) { m_navi = navi; }
    void SetNaviBoss(NaviBoss* naviBoss) { m_naviBoss = naviBoss; }
    NaviBoss* GetNaviBoss() const { return m_naviBoss; }

private:
    void CheckPlayerVsCheckpointLines();
    void CheckPlayerVsEnemies();
    void CheckPlayerVsItems();
    void CheckPlayerProjectilesVsEnemies();
    void CheckPlayerVsTriggerLines();
    void CheckPlayerVsVoidLines();
    bool CheckSphereCollision(const DirectX::XMFLOAT3& posA, const DirectX::XMFLOAT3& posB, float threshold);
    void CheckStageCollision();
    void CheckEnemyProjectilesFull(float elapsedTime);
    void CheckBossFilesVsPlayer();
    void CheckNaviProjectilesVsEnemies(float elapsedTime);
    void CheckNaviBossProjectilesVsPlayer(float elapsedTime);
    void CheckNaviBossProjectilesVsBoss(float elapsedTime); // Fungsi pantulan

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;
    Boss* m_boss = nullptr;

    EnemyManager* m_enemyManager = nullptr;
    ItemManager* m_itemManager = nullptr;
    NaviAlly* m_navi = nullptr;
    NaviBoss* m_naviBoss = nullptr;

    std::function<void(DirectX::XMFLOAT3)> m_onCheckpointReachCallback;
    std::function<void()> m_onLevelCompleteCallback = nullptr;
    std::function<void()> m_onPlayerDeathCallback;
    std::function<void()> m_onPlayerHitCallback = nullptr;
};