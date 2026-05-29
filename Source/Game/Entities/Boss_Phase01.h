#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include "Primitive.h"
#include "UIDialogueBox.h"
#include "BossAIController.h"
#include "IPooledAttackPattern.h"

// Attack param structs — defined in their respective headers
#include "Attack_Radial.h"
#include "Attack_Fan.h"
#include "Attack_Phalanx.h"
#include "Attack_Rain.h"
#include "Attack_Ultimate.h"

#include <vector>
#include <memory>

class Player;

// ============================================================
// Boss_Phase01
//
// Phase 1 of the NaviBoss fight.
// Boss uses a shared bullet pool and a modular attack system.
// Each attack pattern is a self-contained IPooledAttackPattern
// that borrows bullets from the pool owned by this phase.
//
// The AI is driven by BossAIController + State_P1_Idle /
// State_P1_Attacking (see BossStates.h).
//
// Rain runs on an independent parallel track and can overlap
// with any other attack. All other attacks are serialized
// through the state machine (one active at a time).
// ============================================================
class Boss_Phase01 : public INaviPhase {
public:
    explicit Boss_Phase01(Player* target = nullptr);
    ~Boss_Phase01() override = default;

    // ----- INaviPhase Interface -----
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // ----- Attack Management (called by State_P1_Idle) -----
    void AddPooledAttack(std::unique_ptr<IPooledAttackPattern> attack);

    // Overrides from INaviPhase — used by BossAIController states
    bool HasActiveAttacks() const override { return !m_activeAttacks.empty(); }

    // ----- Rain track (independent, queried by AI states) -----
    void TriggerRain(RainMode mode, bool isPositiveSide, float sweepDir = 1.0f);
    bool HasRainActive() const { return m_rainAttack != nullptr && !m_rainAttack->IsFinished(); }

    // ----- Bijuudama parry callback -----
    void OnBijuudamaParried(DirectX::XMFLOAT3 parryPos, NaviBoss* boss);

    // ----- Boss HP -----
    void TakeDamage(int damage, DirectX::XMFLOAT3 hitPos);
    int  GetHP()    const { return m_bossHP; }
    int  GetMaxHP() const { return m_bossMaxHP; }
    bool IsDead()   const { return m_bossHP <= 0; }
    void SetHP(int hp) { m_bossHP = hp; }

    // ----- AI / player -----
    void    SetAITarget(Player* p) { m_aiTarget = p; }
    void    SetAIEnabled(bool val) { m_aiEnabled = val; }
    bool    IsAIEnabled()     const { return m_aiEnabled; }
    Player* GetAITarget()     const { return m_aiTarget; }

    // ----- Param accessors (read by BossStates) -----
    RadialParams& GetRadialParams() { return m_radialParams; }
    const FanParams& GetFanParams() const { return m_fanParams; }
    PhalanxParams& GetPhalanxParams() { return m_phalanxParams; }
    RainParams& GetRainParams() { return m_rainParams; }
    UltimateParams& GetUltimateParams() { return m_ultimateParams; }

    // ----- Bullet pool (read-only access for collision system) -----
    std::vector<std::unique_ptr<Bullet>>& GetProjectiles() { return m_bulletPool; }

private:
    // ----- Boss movement (driven by active attacks) -----
    void UpdateBossMovement(float dt, NaviBoss* boss);

    // ----- Idle hover -----
    void UpdateIdleHover(float dt, NaviBoss* boss);

    // ----- Bullet pool -----
    void UpdateBulletPool(float dt, NaviBoss* boss);

    // ----- Glitch VFX loop -----
    void UpdateGlitchVFX(float dt, NaviBoss* boss);

    // ----- Death sequence -----
    void UpdateDeathSequence(float dt, NaviBoss* boss);

private:
    NaviBoss* m_bossRef = nullptr;

    // ---- Bullet pool ----
    std::vector<std::unique_ptr<Bullet>> m_bulletPool;

    // ---- Attack params ----
    RadialParams   m_radialParams;
    FanParams      m_fanParams;
    PhalanxParams  m_phalanxParams;
    RainParams     m_rainParams;
    UltimateParams m_ultimateParams;

    // ---- Active attacks ----
    // One serialized attack at a time (managed by state machine)
    std::vector<std::unique_ptr<IPooledAttackPattern>> m_activeAttacks;

    // Rain runs in parallel on its own track
    std::unique_ptr<Attack_Rain> m_rainAttack;
    float m_cdRain = 10.0f;

    // ---- AI Director ----
    std::unique_ptr<BossAIController> m_aiController;
    bool    m_aiEnabled = false;
    Player* m_aiTarget = nullptr;

    // ---- Boss movement state ----
    DirectX::XMFLOAT3 m_targetPosition = { 0.0f, 0.0f, 0.0f };
    float             m_moveLerpSpeed = 3.5f;
    float             m_currentMoveLerpSpeed = 0.0f;
    float             m_moveAcceleration = 8.0f;
    float             m_idleHoverTimer = 0.0f;

    // ---- Boss HP & hit flash ----
    int   m_bossMaxHP = 2000;
    int   m_bossHP = 2000;
    float m_hitFlashTimer = 0.0f;

    // ---- Glitch VFX ----
    int   m_bossGlitchVfxHandle = -1;
    float m_bossGlitchVfxTimer = 2.0f;

    // ---- Death sequence ----
    bool  m_isDying = false;
    float m_deathTimer = 0.0f;
    int   m_deathVfxHandle = -1;

    // ---- Opening dialogue ----
    bool m_isOpeningEvent = true;
    bool m_hasSpawnedWindow = false;
    std::unique_ptr<UIDialogueBox> m_dialogueBox;

    // ---- Visual ----
    std::unique_ptr<Primitive> m_zonePrimitive; // Drawn by Attack_Rain, kept here for device lifetime
};