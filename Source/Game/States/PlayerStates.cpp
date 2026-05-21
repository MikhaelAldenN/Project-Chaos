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
#include "NaviBoss.h"

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
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash)
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // --- Logika Attack Baru ---
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SPACE))
    {
        CollisionManager* colMgr = player->GetCollisionManager();
        if (colMgr)
        {
            // 1. Calculate Instantaneous Cursor Direction
            DirectX::XMFLOAT3 pPos{ player->GetMovement()->GetPosition() };
            DirectX::XMFLOAT3 aimPos{ player->GetAimTarget() };

            float aimDx{ aimPos.x - pPos.x };
            float aimDz{ aimPos.z - pPos.z };
            float aimDistSq{ (aimDx * aimDx) + (aimDz * aimDz) };

            DirectX::XMFLOAT3 aimDir{ 0.0f, 0.0f, 1.0f }; // Fallback forward direction

            // Prevent Divide-by-Zero if cursor is exactly under the player's feet
            if (aimDistSq > 0.0001f)
            {
                float aimDist{ std::sqrt(aimDistSq) };
                aimDir = { aimDx / aimDist, 0.0f, aimDz / aimDist };
            }

            // 2. Slash Priority (Cone Check)
            Enemy* slashTarget = colMgr->GetTargetInSlashCone(pPos, aimDir, 0.8f, 0.85f);
            if (slashTarget)
            {
                // NO SNAPPING! We lock the player strictly to their cursor trajectory. 
                player->SetLastValidInput({ aimDir.x, aimDir.z });
                player->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(std::atan2(aimDir.x, aimDir.z)));
                player->SetAimLocked(true);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerSlash>());
                slashTarget->TakeDamage(30);
                return;
            }

            // 3. Parry Priority
            Bullet* parryBullet{ nullptr };
            Enemy* parryTarget{ nullptr };
            if (colMgr->GetParryableProjectile(pPos, 2.0f, &parryBullet, &parryTarget))
            {
                DirectX::XMFLOAT3 bPos{ parryBullet->GetMovement()->GetPosition() };
                float dx{ bPos.x - pPos.x };
                float dz{ bPos.z - pPos.z };
                float dist{ std::sqrt((dx * dx) + (dz * dz)) };

                if (dist > 0.001f) {
                    player->SetLastValidInput({ dx / dist, dz / dist });
                    player->GetMovement()->SetRotationY(XMConvertToDegrees(atan2f(dx, dz)));
                    player->ForceAimTarget(bPos);
                    player->SetAimLocked(true);
                }

                DirectX::XMFLOAT3 tPos = bPos; // Default aman
                float speed = 0.0f;

                if (parryTarget) {
                    // Jika musuh biasa (Teman Anda punya)
                    parryBullet->SetHomingTarget(parryTarget);
                    tPos = parryTarget->GetPosition();
                    speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&parryBullet->GetVelocity()))) * 2.5f;
                    if (speed < 10.0f) speed = 30.0f;
                }
                else if (colMgr->GetNaviBoss()) {
                    // Jika Bijuudama (Milik Bos)
                    parryBullet->SetHomingTarget(nullptr);
                    tPos = pPos; // Tembak ke arah player untuk memicu Shatter!
                    speed = 80.0f;
                }

                // Kalkulasi arah yang aman dari pembagian dengan nol (NaN)
                float dirX = tPos.x - bPos.x;
                float dirZ = tPos.z - bPos.z;
                float distDir = std::sqrt(dirX * dirX + dirZ * dirZ);

                DirectX::XMVECTOR vDir = DirectX::XMVectorSet(0, 0, 1, 0); // Fallback
                if (distDir > 0.001f) {
                    vDir = DirectX::XMVectorSet(dirX / distDir, 0.0f, dirZ / distDir, 0.0f);
                }

                // Logika Homing Deflection
                XMFLOAT3 newVel;
                XMStoreFloat3(&newVel, vDir * speed);
                parryBullet->ApplyMovement(bPos, newVel);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerParry>());
                return;
            }
        }

        // 4. Default: Shoot
        if (!player->GetAnimator()->IsUpperPlaying())
        {
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
            player->FireProjectile();
        }
        return;
    }

    if (player->IsMoving())
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
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
    // Toogle reverse animation
    if (player->IsBackpedaling()) {
        player->GetAnimator()->SetPlaybackSpeed(-1.0f); // Play backward
    }
    else {
        player->GetAnimator()->SetPlaybackSpeed(1.0f);  // Play forward
    }

    // --- 1. Cek Transisi Dash ---
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash)
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // --- 2. Cek Logika Serangan (Identik dengan Idle) ---
    // --- Logika Attack Baru ---
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SPACE))
    {
        CollisionManager* colMgr = player->GetCollisionManager();
        if (colMgr)
        {
            // 1. Calculate Instantaneous Cursor Direction
            DirectX::XMFLOAT3 pPos{ player->GetMovement()->GetPosition() };
            DirectX::XMFLOAT3 aimPos{ player->GetAimTarget() };

            float aimDx{ aimPos.x - pPos.x };
            float aimDz{ aimPos.z - pPos.z };
            float aimDistSq{ (aimDx * aimDx) + (aimDz * aimDz) };

            DirectX::XMFLOAT3 aimDir{ 0.0f, 0.0f, 1.0f }; // Fallback forward direction

            // Prevent Divide-by-Zero if cursor is exactly under the player's feet
            if (aimDistSq > 0.0001f)
            {
                float aimDist{ std::sqrt(aimDistSq) };
                aimDir = { aimDx / aimDist, 0.0f, aimDz / aimDist };
            }

            // 2. Slash Priority (Cone Check)
            Enemy* slashTarget = colMgr->GetTargetInSlashCone(pPos, aimDir, 0.8f, 0.85f);
            if (slashTarget)
            {
                // NO SNAPPING! We lock the player strictly to their cursor trajectory. 
                player->SetLastValidInput({ aimDir.x, aimDir.z });
                player->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(std::atan2(aimDir.x, aimDir.z)));
                player->SetAimLocked(true);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerSlash>());
                slashTarget->TakeDamage(30);
                return;
            }

            // 3. Parry Priority
            Bullet* parryBullet{ nullptr };
            Enemy* parryTarget{ nullptr };
            if (colMgr->GetParryableProjectile(pPos, 2.0f, &parryBullet, &parryTarget))
            {
                DirectX::XMFLOAT3 bPos{ parryBullet->GetMovement()->GetPosition() };
                float dx{ bPos.x - pPos.x };
                float dz{ bPos.z - pPos.z };
                float dist{ std::sqrt((dx * dx) + (dz * dz)) };

                if (dist > 0.001f) {
                    float dirX{ dx / dist };
                    float dirZ{ dz / dist };

                    player->SetLastValidInput({ dirX, dirZ });
                    player->GetMovement()->SetRotationY(DirectX::XMConvertToDegrees(std::atan2(dx, dz)));

                    // Aim THROUGH the bullet so we don't snap backward if we overshoot!
                    DirectX::XMFLOAT3 aimThrough{ pPos.x + (dirX * 50.0f), bPos.y, pPos.z + (dirZ * 50.0f) };
                    player->ForceAimTarget(aimThrough);
                    player->SetAimLocked(true);
                }

                // Logika Homing Deflection
                DirectX::XMFLOAT3 tPos = bPos;
                float speed = 0.0f;

                if (parryTarget) {
                    parryBullet->SetHomingTarget(parryTarget);
                    tPos = parryTarget->GetPosition();
                    speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&parryBullet->GetVelocity()))) * 2.5f;
                    if (speed < 10.0f) speed = 30.0f;
                }
                else if (colMgr->GetNaviBoss()) {
                    parryBullet->SetHomingTarget(nullptr);
                    tPos = pPos;
                    speed = 80.0f;
                }

                float dirX = tPos.x - bPos.x;
                float dirZ = tPos.z - bPos.z;
                float distDir = std::sqrt(dirX * dirX + dirZ * dirZ);

                DirectX::XMVECTOR vDir = DirectX::XMVectorSet(0, 0, 1, 0);
                if (distDir > 0.001f) {
                    vDir = DirectX::XMVectorSet(dirX / distDir, 0.0f, dirZ / distDir, 0.0f);
                }

                XMFLOAT3 newVel;
                XMStoreFloat3(&newVel, vDir * speed);
                parryBullet->ApplyMovement(bPos, newVel);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerParry>());
                return;
            }
        }

        // 4. Default: Shoot
        if (!player->GetAnimator()->IsUpperPlaying())
        {
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
            player->FireProjectile();
        }
        return;
    }

    // --- 3. Cek Transisi Kembali ke Idle ---
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
    player->dashCooldownTimer = PlayerConst::DashCooldown;
    player->TriggerInvincibility(DASH_IFRAME_DURATION);
}

void PlayerDash::Update(Player* player, float dt)
{
    timer -= dt;

    player->GetMovement()->SetVelocity({
        dashDir.x * player->GetDashSpeed(),
        0.0f,
        dashDir.y * player->GetDashSpeed()
        });

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
}

void PlayerShoot::Update(Player* player, float dt)
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

void PlayerShoot::Exit(Player* player)
{
}