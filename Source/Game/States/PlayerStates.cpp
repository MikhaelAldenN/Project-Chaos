#include "PlayerStates.h"
#include "Player.h"
#include "PlayerConstants.h"
#include "StateMachine.h"
#include "AnimationController.h"
#include "System/Input.h"
#include <memory>
#include <cmath>

#include "System/CollisionManager.h"
#include "Enemy.h"
#include "Bullet.h"
#include "Boss.h"
#include "System/AudioManager.h"

#include "EffectManager.h"

namespace {
    // [[nodiscard]] forces the caller to check if a state transition actually happened.
    // Passed by raw pointer because the state machine doesn't own the player.
    [[nodiscard]] bool TryExecuteCombatAction(Player* player, bool allowShoot = true)
    {
        // 1. Anticipate Nullptr Bug: Always validate pointers before dereferencing.
        if (!player || !player->IsInputEnabled()) return false;

        // 2. The Input Bug Fix: We use VK_LBUTTON for Left Mouse Button.
        // NOTE: If your Input::Instance() separates Keyboard and Mouse, change GetKeyboard() to GetMouse()!
        // Using GetKeyboard() to check a mouse click is semantically dangerous.
        auto& input{ Input::Instance().GetKeyboard() };

        const bool isShootInput{ input.IsPress(VK_LBUTTON) };

        if (!isShootInput) return false;

        CollisionManager* const colMgr{ player->GetCollisionManager() };
        if (colMgr)
        {
            // 3. Brace Initialization prevents narrowing conversions and garbage data.
            const DirectX::XMFLOAT3 pPos{ player->GetMovement()->GetPosition() };
            const DirectX::XMFLOAT3 aimPos{ player->GetAimTarget() };

            const float aimDx{ aimPos.x - pPos.x };
            const float aimDz{ aimPos.z - pPos.z };
            const float aimDistSq{ (aimDx * aimDx) + (aimDz * aimDz) };

            DirectX::XMFLOAT3 aimDir{ 0.0f, 0.0f, 1.0f }; // Fallback forward direction

            // 4. Anticipate Math Error: Prevent Divide-by-Zero
            if (aimDistSq > 0.0001f)
            {
                const float aimDist{ std::sqrt(aimDistSq) };
                aimDir = { aimDx / aimDist, 0.0f, aimDz / aimDist };
            }

            // --- Slash Priority ---
            if (Enemy * slashTarget{ colMgr->GetTargetInSlashCone(pPos, aimDir, 0.8f, 0.85f) })
            {
                player->SetLastValidInput({ aimDir.x, aimDir.z });
                player->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(std::atan2(aimDir.x, aimDir.z)));
                player->SetAimLocked(true);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerSlash>());
                slashTarget->TakeDamage(30);
                return true;
            }

            // --- Parry Priority ---
            Bullet* parryBullet{ nullptr };
            Enemy* parryTarget{ nullptr };

            if (colMgr->GetParryableProjectile(pPos, 2.0f, &parryBullet, &parryTarget))
            {
                const DirectX::XMFLOAT3 bPos{ parryBullet->GetMovement()->GetPosition() };
                const float dx{ bPos.x - pPos.x };
                const float dz{ bPos.z - pPos.z };
                const float dist{ std::sqrt((dx * dx) + (dz * dz)) };

                if (dist > 0.001f)
                {
                    const float dirX{ dx / dist };
                    const float dirZ{ dz / dist };

                    player->SetLastValidInput({ dirX, dirZ });
                    player->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(std::atan2(dx, dz)));

                    // Aim THROUGH the bullet so we don't snap backward if we overshoot
                    const DirectX::XMFLOAT3 aimThrough{ pPos.x + (dirX * 50.0f), bPos.y, pPos.z + (dirZ * 50.0f) };
                    player->ForceAimTarget(aimThrough);
                    player->SetAimLocked(true);
                }

                DirectX::XMFLOAT3 tPos{ bPos };
                float speed{ 0.0f };

                if (parryTarget) {
                    parryBullet->SetHomingTarget(parryTarget);
                    tPos = parryTarget->GetPosition();
                    // Avoid unnecessary vector copies; use XMLoadFloat3 safely.
                    speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&parryBullet->GetVelocity()))) * 2.5f;
                    if (speed < 10.0f) speed = 30.0f;
                }
                else if (colMgr->GetBoss()) {
                    parryBullet->SetHomingTarget(nullptr);
                    tPos = pPos;
                    speed = 80.0f;
                    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Bijuudama_Shoot.wav", 0.2f);
                }

                const float dirX{ tPos.x - bPos.x };
                const float dirZ{ tPos.z - bPos.z };
                const float distDir{ std::sqrt((dirX * dirX) + (dirZ * dirZ)) };

                DirectX::XMVECTOR vDir{ DirectX::XMVectorSet(0, 0, 1, 0) };
                if (distDir > 0.001f) {
                    vDir = DirectX::XMVectorSet(dirX / distDir, 0.0f, dirZ / distDir, 0.0f);
                }

                DirectX::XMFLOAT3 newVel;
                DirectX::XMStoreFloat3(&newVel, DirectX::XMVectorScale(vDir, speed));
                parryBullet->ApplyMovement(bPos, newVel);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerParry>());
                return true;
            }
        }

        // --- Default: Shoot ---
        if (allowShoot && !player->GetAnimator()->IsUpperPlaying())
        {
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
            player->FireProjectile();
            return true;
        }

        return false;
    }
}

using namespace DirectX;

// ============================================================
// IDLE
// ============================================================
void PlayerIdle::Enter(Player* player)
{
    player->GetAnimator()->SetPlaybackSpeed(1.0f);
    player->GetAnimator()->Play("Idle", true, PlayerConst::AnimBlendDefault);
}

void PlayerIdle::Update(Player* player, float dt)
{
    if (!player->IsInputEnabled()) return;

    // 1. Dash Priority
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && (player->canDash || player->IsPowerUncapped()))
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // 2. Combat Priority (Handled by our DRY helper!)
    if (TryExecuteCombatAction(player)) return;

    // 3. Movement Fallback
    if (player->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
    }
}

// ============================================================
// MOVING
// ============================================================
void PlayerMoving::Enter(Player* player)
{
    player->GetAnimator()->Play("RunPistol", true, PlayerConst::AnimBlendDefault);
}

void PlayerMoving::Update(Player* player, float dt)
{
    // Handle Animation direction
    player->GetAnimator()->SetPlaybackSpeed(player->IsBackpedaling() ? -1.0f : 1.0f);

    if (player->IsInputEnabled())
    {
        // 1. Dash Priority
        if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && (player->canDash || player->IsPowerUncapped()))
        {
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
            return;
        }

        // 2. Combat Priority (Perfectly synchronized with Idle!)
        if (TryExecuteCombatAction(player)) return;
    }

    // 3. Idle Fallback
    if (!player->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}

// ============================================================
// DASH
// ============================================================

void PlayerDash::Enter(Player* player)
{
    constexpr float DASH_IFRAME_DURATION = 0.2f;

    timer = player->GetDashDuration();
    dashDir = player->GetLastValidInput();

    player->canDash = false;
    player->dashCooldownTimer = player->GetDashCooldown();
    player->TriggerInvincibility(DASH_IFRAME_DURATION);


    // =========================================================
    // Play VFX Dash Go dan sesuaikan arah rotasinya!
    // =========================================================
    DirectX::XMFLOAT3 pos = player->GetMovement()->GetPosition();
    pos.y += 1.0f; // Naikkan sedikit agar pas di tengah badan

    // [MODIFIKASI] Simpan handle ke variabel class
    m_dashGoVfxHandle = EffectManager::Instance().Play("Data/Effect/VFX_Player_Dash_Go.efk", pos, 0.2f);

    if (m_dashGoVfxHandle != -1) {
        // [FIX] Tambahkan DirectX::XM_PI (180 derajat dalam radian) untuk membalik arahnya!
        float yaw = atan2f(dashDir.x, dashDir.y) + DirectX::XM_PI;

        EffectManager::Instance().SetRotation(m_dashGoVfxHandle, { 0.0f, yaw, 0.0f });
    }

    std::string dashSounds[] = {
        "Data/Sound/SE_Dash_01.wav",
        "Data/Sound/SE_Dash_02.wav",
        "Data/Sound/SE_Dash_03.wav"
    };

    // 2. Pilih index secara acak (0, 1, atau 2)
    int randomIndex = rand() % 3;

    // 3. Mainkan suaranya lewat AudioManager
    // Kita gunakan volume 0.5f agar tidak terlalu memekakkan telinga
    AudioManager::Instance().PlaySFX(dashSounds[randomIndex], 0.1f);
}

void PlayerDash::Update(Player* player, float dt)
{
    timer -= dt;

    player->GetMovement()->SetVelocity({
        dashDir.x * player->GetDashSpeed(),
        0.0f,
        dashDir.y * player->GetDashSpeed()
        });

    // =========================================================
    // [BARU] Terus seret VFX mengikuti posisi player selama Dash berjalan
    // =========================================================
    if (m_dashGoVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_dashGoVfxHandle))
    {
        DirectX::XMFLOAT3 trackPos = player->GetMovement()->GetPosition();
        trackPos.y += 1.0f; // Pastikan offset Y sama dengan saat Enter
        EffectManager::Instance().SetPosition(m_dashGoVfxHandle, trackPos);
    }

    if (timer <= 0.0f)
    {
        if (player->IsMoving())
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
        else
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}

void PlayerDash::Exit(Player* player)
{
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}

// ============================================================
// SLASH
// ============================================================

void PlayerSlash::Enter(Player* player)
{
    player->SetActiveWeapon(Player::WeaponType::Sword);
    player->GetAnimator()->PlayUpper("Parry", false);

    float yawRad = XMConvertToRadians(player->GetMovement()->GetRotation().y);

    player->GetMovement()->SetVelocity({
        sinf(yawRad) * PlayerConst::SlashLungeForce,
        0.0f,
        cosf(yawRad) * PlayerConst::SlashLungeForce
        });
}

void PlayerSlash::Update(Player* player, float dt)
{
    timer -= dt;

    // Decelerate lunge over time
    XMFLOAT3 vel = player->GetMovement()->GetVelocity();
    vel.x *= PlayerConst::SlashDrag;
    vel.z *= PlayerConst::SlashDrag;
    player->GetMovement()->SetVelocity(vel);

    if (timer <= 0.0f)
    {
        if (player->IsMoving())
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
        else
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}

void PlayerSlash::Exit(Player* player)
{
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
}

// ============================================================
// PARRY
// ============================================================

void PlayerParry::Enter(Player* player)
{
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    player->GetAnimator()->PlayUpper("Parry", false);
    player->SetActiveWeapon(Player::WeaponType::Sword);
}

void PlayerParry::Update(Player* player, float dt)
{
    timer -= dt;

    if (timer <= 0.0f)
    {
        if (player->IsMoving())
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
        else
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}

void PlayerParry::Exit(Player* player)
{
}

// ============================================================
// SHOOT
// ============================================================

void PlayerShoot::Enter(Player* player)
{
    // The first shot was already fired by TryExecuteCombatAction before entering.
    // We pass 'false' because this is the initial trigger, not a held loop.
    PerformShootInternal(player, false);
}

void PlayerShoot::Update(Player* player, float dt)
{
    // 1. Dash Lockout Prevention
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && (player->canDash || player->IsPowerUncapped()))
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    timer -= dt;

    if (timer <= 0.0f)
    {
        auto& input{ Input::Instance().GetKeyboard() };

        if (input.IsPress(VK_LBUTTON))
        {
            // 2. Melee Proximity Override
            if (TryExecuteCombatAction(player, false)) return;

            // 3. Lower-Body Animation Sync
            if (player->IsMoving()) {
                player->GetAnimator()->SetPlaybackSpeed(player->IsBackpedaling() ? -1.0f : 1.0f);
                if (!player->GetAnimator()->IsPlaying("RunPistol")) {
                    player->GetAnimator()->Play("RunPistol", true, PlayerConst::AnimBlendDefault);
                }
            }
            else {
                player->GetAnimator()->SetPlaybackSpeed(1.0f);
                if (!player->GetAnimator()->IsPlaying("Idle")) {
                    player->GetAnimator()->Play("Idle", true, PlayerConst::AnimBlendDefault);
                }
            }

            // 4. Fire and loop internally
            player->FireProjectile();

            // [MODIFIED] We are now looping, so we flag isHeld as true
            PerformShootInternal(player, true);
        }
        else
        {
            if (player->IsMoving())
                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
            else
                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
        }
    }
}

void PlayerShoot::PerformShootInternal(Player* player, bool isHeld)
{
    // Zero-overhead array initialization
    const std::string shootSounds[]{
        "Data/Sound/SE_Player_Shoot_01.wav",
        "Data/Sound/SE_Player_Shoot_02.wav",
        "Data/Sound/SE_Player_Shoot_03.wav"
    };

    const int randomIndex{ rand() % 3 };
    AudioManager::Instance().PlaySFX(shootSounds[randomIndex], 0.1f);

    float currentDelay{ player->GetShootDelay() };

    // --- FIRE RATE LOGIC TREE ---
    if (player->IsPowerUncapped())
    {
        // Absolute Priority: Overdrive bypasses all penalties
        currentDelay = 0.05f;
    }
    else if (isHeld)
    {
        // Compile-time constant for the penalty multiplier.
        // A value of 1.5f means firing is 50% slower when holding the button.
        // Adjust this variable to tune the game feel.
        constexpr float HOLD_PENALTY_MULTIPLIER{ 2.5f };
        currentDelay *= HOLD_PENALTY_MULTIPLIER;
    }

    timer = currentDelay;
}

void PlayerShoot::Exit(Player* player)
{
}