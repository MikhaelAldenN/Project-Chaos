#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include "Primitive.h"
#include "UIDialogueBox.h"
#include <vector>
#include <memory>
#include "EffectManager.h"

class Player;

// ============================================================
// NaviBulletParams - Centralized tuning values for all attacks.
//                   Tweak here without touching gameplay code.
// ============================================================
struct NaviBulletParams {
    // ----- General -----
    float screenDespawnPadding = 5.0f;
    float sfxVolumeMultiplier = 1.0f;
    float bgmVolumeMultiplier = 1.0f;

    // ----- Radial Burst -----
    float radialSpeed = 20.0f; // [変更] speed から radialSpeed に名前変更
    int   count = 24;
    float fireRate = 1.5f;
    float burstDelay = 0.156f;
    DirectX::XMFLOAT4 color = { 1.0f, 0.2f, 0.2f, 1.0f };
    float despawnDist = 60.0f;

    // ----- Targeted Fan Burst (Shotgun) -----
    float fanSpeed = 25.0f;    // [追加] Fan Burst 専用の弾速
    int   fanLines = 4;
    int   fanWaves = 4;
    float fanWaveDelay = 0.176f;
    float fanSpreadAngle = 0.183f;

    // ----- Rhythm Laser (Bijuudama charge indicator) -----
    float laserDuration = 4.0f;
    float laserParryWindow = 0.5f;
    float laserStartRadius = 8.0f;
    float laserTargetRadius = 1.5f;
    int   laserDamage = 20;

    // ----- Bijuudama (Charged Energy Ball) -----
    DirectX::XMFLOAT4 bijuudamaColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    float bijuudamaBaseHitbox = 0.5f;
    float bijuudamaMaxHitboxGrow = 2.0f;
    float bijuudamaVisualMultiplier = 4.0f;
    float bijuudamaSpawnOffsetZ = 2.0f;
    float bijuudamaShootSpeed = 45.0f;
    float bijuudamaAttackMoveSpeed = 3.0f; // Boss slide speed during charge
    float bijuudamaReturnMoveSpeed = 2.0f; // Boss return speed after fire
    float bijuudamaPostFireDelay = 1.0f;

    // ----- Shatter Bijuudama (Parry burst fragments) -----
    int   shatterMinFragments = 5;
    int   shatterMaxFragments = 8;
    float shatterMinRadius = 0.2f;
    float shatterMaxRadius = 0.8f;
    float shatterMinDuration = 0.5f; // Travel time (higher = slower)
    float shatterMaxDuration = 0.7f;
    float shatterCurveOffset = 11.0f; // Parabola width / arc strength

    // ----- Glintstone Phalanx (Orbiting homing swords) -----
    int   phalanxCount = 7;
    float phalanxChargeDelay = 0.2f;  // Delay between each bullet summon
    float phalanxHoldDuration = 2.0f;
    float phalanxFireDelay = 0.0f;  // Delay between each shot
    float phalanxSpeed = 80.0f;
    float phalanxHoverRadius = 4.0f;  // Orbit distance from boss
    float phalanxTurnSpeed = 1.5f;  // Weak homing (so player can dash)
    float phalanxSmoothSpeed = 12.0f;
    float phalanxPostFireDelay = 1.0f;
    float phalanxAttackMoveSpeed = 2.0f;
    float phalanxReturnMoveSpeed = 2.0f;

    // ----- Asgore Rain (Area Denial) -----
    float rainMinSpeed = 50.0f; // [追加] 雨の最小落下速度
    float rainMaxSpeed = 90.0f; // [追加] 雨の最大落下速度
    float rainWarningDuration = 1.5f;
    float rainActiveDuration = 2.0f;
    float rainWidth = 25.0f;
    float rainDepth = 40.0f;
    int   rainDamage = 1;
};

// ============================================================
// NaviPhaseNormal - Main boss fight phase.
//                   Runs the AI director and all attack patterns.
// ============================================================
class NaviPhaseNormal : public INaviPhase {
public:
    NaviPhaseNormal(Player* target = nullptr);
    ~NaviPhaseNormal() override = default;

    // ----- INaviPhase Interface -----
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // ----- Attack Triggers -----
    void TriggerTripleBurst();
    void TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos);
    void TriggerBijuudama(Player* targetPlayer);
    void TriggerPhalanx(Player* targetPlayer);
    void TriggerRainAttack(bool isVertical, bool isPositiveSide, float sweepDir = 1.0f, bool isDual = false);
    void ShatterBijuudama(DirectX::XMFLOAT3 parryPos, NaviBoss* boss);

    // Cancel an in-progress bijuudama (e.g. on phase transition)
    void CancelBijuudama() {
        if (m_chargeEffectHandle != -1) {
            EffectManager::Instance().Stop(m_chargeEffectHandle);
            m_chargeEffectHandle = -1;
        }
        
        m_isLaserLocked = false;
        m_bijuudamaBall = nullptr;
        m_laserTimer = 0.0f;
        m_isBijuudamaRecovering = true;
        m_bijuudamaRecoveryTimer = 0.0f;
    }

    // ----- Rain State Accessors (used by collision/scene code) -----
    int               GetRainState()    const { return m_rainState; }
    bool              IsRainVertical()  const { return m_rainIsVertical; }
    bool              IsRainDual()      const { return m_rainIsDual; }
    DirectX::XMFLOAT3 GetRainCenter()   const { return m_rainCenter; }
    DirectX::XMFLOAT3 GetRainCenter2()  const { return m_rainCenter2; }

    float GetActualRainWidth() const {
        if (m_rainIsDual)    return 18.0f;
        return m_rainIsVertical ? 25.0f : 80.0f;
    }
    float GetActualRainDepth() const { return m_rainIsVertical ? 45.0f : 15.0f; }

    // ----- Bijuudama State Accessors -----
    bool  IsLaserLocked()  const { return m_isLaserLocked; }
    float GetLaserTimer()  const { return m_laserTimer; }

    // ----- Boss HP -----
    void TakeDamage(int damage, DirectX::XMFLOAT3 hitPos);
    void SetHP(int hp) { m_bossHP = hp; } // <--- [追加] これでエラーが直ります！
    int  GetHP()    const { return m_bossHP; }
    int  GetMaxHP() const { return m_bossMaxHP; }
    bool IsDead()   const { return m_bossHP <= 0; }

    // ----- AI Director -----
    void SetAITarget(Player* p) { m_aiTarget = p; }
    void SetAIEnabled(bool val) { m_aiEnabled = val; }
    bool IsAIEnabled() const { return m_aiEnabled; }

    // ----- Data Access -----
    NaviBulletParams& GetParams() { return m_params; }
    std::vector<std::unique_ptr<Bullet>>& GetProjectiles() { return m_bulletPool; }

private:
    // ----- Internal Fire Helpers -----
    void FireRadialBurst(NaviBoss* boss, float angleOffset);
    void FireFanWave(NaviBoss* boss);

    // ----- AI Director Brain -----
    void UpdateAI(float dt, NaviBoss* boss);

private:
    // ----- Bullet Pool -----
    std::vector<std::unique_ptr<Bullet>> m_bulletPool;
    NaviBulletParams m_params;

    // ----- Radial Burst State -----
    bool  m_isFiring = false;
    int   m_burstsFired = 0;
    float m_burstSequenceTimer = 0.0f;

    // ----- Fan Burst State -----
    bool  m_isFiringFan = false;
    int   m_fanWavesFired = 0;
    float m_fanTimer = 0.0f;
    float m_lockedBaseAngle = 0.0f; // Angle to player at the moment of trigger

    // ----- Bijuudama / Rhythm Laser State -----
    bool    m_isLaserLocked = false;
    float   m_laserTimer = 0.0f;
    Player* m_laserTargetPlayer = nullptr;
    Bullet* m_bijuudamaBall = nullptr;
    bool    m_isBijuudamaRecovering = false;
    float   m_bijuudamaRecoveryTimer = 0.0f;

    int m_chargeEffectHandle = -1;
    int m_bossGlitchVfxHandle = -1;
    float m_bossGlitchVfxTimer = 2.0f;

    // ----- Glintstone Phalanx State -----
    // States: 0=Idle, 1=Charging, 2=Holding, 3=Firing, 4=PostFire, 5=Returning
    int   m_phalanxState = 0;
    float m_phalanxTimer = 0.0f;
    int   m_phalanxSpawned = 0;
    int   m_phalanxFired = 0;
    Player* m_phalanxTarget = nullptr;
    std::vector<Bullet*> m_phalanxBullets; // Raw pointers into the bullet pool

    // ----- Asgore Rain State -----
    // States: 0=Off, 1=Warning, 2=Raining, 3=Dissipating
    int               m_rainState = 0;
    float             m_rainTimer = 0.0f;
    bool              m_rainIsVertical = false;
    bool              m_rainIsDual = false;
    float             m_rainSweepDir = 1.0f;
    DirectX::XMFLOAT3 m_rainCenter = { 0, 0, 0 };
    DirectX::XMFLOAT3 m_rainCenter2 = { 0, 0, 0 };

    // ----- Danger Zone Visual -----
    std::unique_ptr<Primitive> m_zonePrimitive;

    // ----- Boss HP & Hit Flash -----
    int   m_bossMaxHP = 1500;
    int   m_bossHP = 1500;
    float m_hitFlashTimer = 0.0f;

    // ----- AI Director -----
    bool    m_aiEnabled = false;
    Player* m_aiTarget = nullptr;

    // Attack cooldowns (seconds until next use)
    float m_cdRadial = 2.0f;
    float m_cdFan = 4.0f;
    float m_cdPhalanx = 6.0f;
    float m_cdRain = 10.0f;
    float m_cdBijuudama = 15.0f;

    // Minimum rest time between any two attacks
    float m_aiGlobalCooldown = 0.5f;

    // ----- Boss Movement -----
    DirectX::XMFLOAT3 m_targetPosition = { 0.0f, 0.0f, 0.0f };
    float m_moveLerpSpeed = 3.5f;
    float m_currentMoveLerpSpeed = 0.0f;
    float m_moveAcceleration = 8.0f;
    float m_idleHoverTimer = 0.0f; // Drives the Lissajous idle curve

    // ----- Audio -----
    float m_rainSFXTimer = 0.0f;
    const float RAIN_SFX_LOOP_DURATION = 0.8f;

    // ----- Opening Sequence -----
    bool m_isOpeningEvent = true;
    bool m_hasSpawnedWindow = false;
    std::unique_ptr<UIDialogueBox> m_dialogueBox;
};