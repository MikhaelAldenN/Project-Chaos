#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>

// --- Phase 1 Attacks ---
#include "AttackRadial.h"
#include "AttackFan.h"
#include "AttackPhalanx.h"
#include "AttackRain.h"
#include "AttackUltimate.h"

// --- Phase 2 Attacks ---
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"

class BossPhase01;
class BossPhase02;
class Boss;
class Player;

// ========================================================
// BOSS AI - PHASE 01
// ========================================================
class BossAI_Phase01 {
public:

    enum class AttackSequence {
        Radial,
        Fan,
        Phalanx,
        RadialStream,
        FanTripple,
        Rain,
        Ultimate
    };

    BossAI_Phase01(BossPhase01* phase, Player* target);
    void Update(float dt, Boss* boss);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void SetTarget(Player* target) { m_target = target; }

    // Akses Parameter untuk ImGui
    RadialParams& GetRadialParams() { return m_radialParams; }
    FanParams& GetFanParams() { return m_fanParams; }
    PhalanxParams& GetPhalanxParams() { return m_phalanxParams; }
    RainParams& GetRainParams() { return m_rainParams; }
    UltimateParams& GetUltimateParams() { return m_ultimateParams; }

private:
    BossPhase01* m_phase = nullptr;
    Player* m_target = nullptr;
    bool          m_enabled = false;

    // Parameter
    RadialParams   m_radialParams;
    FanParams      m_fanParams;
    PhalanxParams  m_phalanxParams;
    RainParams     m_rainParams;
    UltimateParams m_ultimateParams;

    // Sistem Antrean (Sequence)
    AttackSequence m_currentAttack = AttackSequence::Radial;
    float m_cooldownTimer = 2.0f;
};

// ========================================================
// BOSS AI - PHASE 02 (WINDOWKILL)
// ========================================================
class BossAI_Phase02 {
public:

    enum class AttackSequence {
        Bouncing,
        Boomerang,
        Blaster,
        Spear
    };

    BossAI_Phase02(BossPhase02* phase, Player* target);
    void Update(float dt, Boss* boss);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void SetTarget(Player* target) { m_target = target; }

    // Akses Parameter untuk ImGui
    BouncingBulletParams& GetBouncingParams() { return m_bouncingParams; }
    BoomerangParams& GetBoomerangParams() { return m_boomerangParams; }
    BlasterParams& GetBlasterParams() { return m_blasterParams; }
    UndyneSpearParams& GetUndyneParams() { return m_undyneParams; }

private:
    BossPhase02* m_phase = nullptr;
    Player* m_target = nullptr;
    bool          m_enabled = false;

    // Parameter
    BouncingBulletParams m_bouncingParams;
    BoomerangParams      m_boomerangParams;
    BlasterParams        m_blasterParams;
    UndyneSpearParams    m_undyneParams;

    // Sistem Antrean (Sequence)
    AttackSequence m_currentAttack = AttackSequence::Bouncing;
    float m_cooldownTimer = 3.0f; // Jeda awal saat masuk fase
};