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
        RadialContinuos,
        Fan,
        FanContinuos,
        Phalanx,
        Rain,
        Ultimate
    };

    BossAI_Phase01(BossPhase01* phase, Player* target);
    void Update(float dt, Boss* boss);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void SetTarget(Player* target) { m_target = target; }

    // CATATAN: Semua fungsi Get...Params() DIHAPUS karena sudah pindah ke ParamManager

private:
    BossPhase01* m_phase = nullptr;
    Player* m_target = nullptr;
    bool          m_enabled = false;

    // Sistem Antrean (Sequence)
    AttackSequence m_currentAttack = AttackSequence::Phalanx;
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

    // CATATAN: Semua fungsi Get...Params() DIHAPUS karena sudah pindah ke ParamManager

private:
    BossPhase02* m_phase = nullptr;
    Player* m_target = nullptr;
    bool          m_enabled = false;

    // Sistem Antrean (Sequence)
    AttackSequence m_currentAttack = AttackSequence::Bouncing;
    float m_cooldownTimer = 3.0f; // Jeda awal saat masuk fase
};