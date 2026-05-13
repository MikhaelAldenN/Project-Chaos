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
            XMFLOAT3 pPos = player->GetMovement()->GetPosition();
            Enemy* slashTarget = nullptr;
            Bullet* parryBullet = nullptr;
            Enemy* parryTarget = nullptr;

            // 1. Slash Priority
            if (colMgr->GetTargetInSlashRange(pPos, 0.8f, &slashTarget))
            {
                XMFLOAT3 ePos = slashTarget->GetPosition();
                float dx = ePos.x - pPos.x;
                float dz = ePos.z - pPos.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.001f) {
                    player->SetLastValidInput({ dx / dist, dz / dist });
                    player->GetMovement()->SetRotationY(XMConvertToDegrees(atan2f(dx, dz)));
                    player->ForceAimTarget(ePos);
                    player->SetAimLocked(true);
                }

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerSlash>());
                slashTarget->TakeDamage(30);
                return;
            }

            // 2. Parry Priority
            if (colMgr->GetParryableProjectile(pPos, 2.0f, &parryBullet, &parryTarget))
            {
                XMFLOAT3 bPos = parryBullet->GetMovement()->GetPosition();
                float dx = bPos.x - pPos.x;
                float dz = bPos.z - pPos.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.001f) {
                    player->SetLastValidInput({ dx / dist, dz / dist });
                    player->GetMovement()->SetRotationY(XMConvertToDegrees(atan2f(dx, dz)));
                    player->ForceAimTarget(bPos);
                    player->SetAimLocked(true);
                }

                XMFLOAT3 tPos;
                float speed = 40.0f; // Kecepatan khusus Counter Bullet

                if (parryTarget) {
                    // Jika musuh biasa
                    parryBullet->SetHomingTarget(parryTarget);
                    tPos = parryTarget->GetPosition();
                    speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&parryBullet->GetVelocity()))) * 2.5f;
                    if (speed < 10.0f) speed = 30.0f;
                }
                else if (colMgr->GetNaviBoss()) {
                    // [FIX] JIKA BIJUUDAMA SUKSES: Tembak ke arah Player dengan kecepatan ekstrem!
                    tPos = pPos; // Jadikan player sebagai target!
                    speed = 80.0f;
                }

                XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&tPos) - XMLoadFloat3(&bPos));
                XMFLOAT3 newVel;
                XMStoreFloat3(&newVel, vDir * speed);
                parryBullet->ApplyMovement(bPos, newVel);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerParry>());
                return;
            }
        }

        // 3. Default: Shoot
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
        player->FireProjectile();
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
    // --- 1. Cek Transisi Dash ---
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SHIFT) && player->canDash)
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // --- 2. Cek Logika Serangan (Identik dengan Idle) ---
    if (Input::Instance().GetKeyboard().IsTriggered(VK_SPACE))
    {
        CollisionManager* colMgr = player->GetCollisionManager();
        if (colMgr)
        {
            XMFLOAT3 pPos = player->GetMovement()->GetPosition();
            Enemy* slashTarget = nullptr;
            Bullet* parryBullet = nullptr;
            Enemy* parryTarget = nullptr;

            // 2A. Slash Priority
            if (colMgr->GetTargetInSlashRange(pPos, 0.8f, &slashTarget))
            {
                XMFLOAT3 ePos = slashTarget->GetPosition();
                float dx = ePos.x - pPos.x;
                float dz = ePos.z - pPos.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.001f) {
                    player->SetLastValidInput({ dx / dist, dz / dist });
                    player->GetMovement()->SetRotationY(XMConvertToDegrees(atan2f(dx, dz)));
                    player->ForceAimTarget(ePos);
                    player->SetAimLocked(true);
                }

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerSlash>());
                slashTarget->TakeDamage(30);
                return;
            }

            // 2B. Parry Priority
            if (colMgr->GetParryableProjectile(pPos, 2.0f, &parryBullet, &parryTarget))
            {
                XMFLOAT3 bPos = parryBullet->GetMovement()->GetPosition();
                float dx = bPos.x - pPos.x;
                float dz = bPos.z - pPos.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.001f) {
                    player->SetLastValidInput({ dx / dist, dz / dist });
                    player->GetMovement()->SetRotationY(XMConvertToDegrees(atan2f(dx, dz)));
                    player->ForceAimTarget(bPos);
                    player->SetAimLocked(true);
                }

                XMFLOAT3 tPos;
                float speed = 40.0f; // Kecepatan khusus Counter Bullet

                if (parryTarget) {
                    // Jika musuh biasa
                    parryBullet->SetHomingTarget(parryTarget);
                    tPos = parryTarget->GetPosition();
                    speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&parryBullet->GetVelocity()))) * 2.5f;
                    if (speed < 10.0f) speed = 30.0f;
                }
                else if (colMgr->GetNaviBoss()) {
                    // [FIX] JIKA BIJUUDAMA SUKSES: Tembak ke arah Player dengan kecepatan ekstrem!
                    tPos = pPos; // Jadikan player sebagai target!
                    speed = 80.0f;
                }

                XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&tPos) - XMLoadFloat3(&bPos));
                XMFLOAT3 newVel;
                XMStoreFloat3(&newVel, vDir * speed);
                parryBullet->ApplyMovement(bPos, newVel);

                player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerParry>());
                return;
            }
        }

        // 2C. Default: Shoot
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
        player->FireProjectile();
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
    player->color = { 1.0f, 0.0f, 0.0f, 1.0f }; // Flash red

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
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    player->SetAimLocked(false);
}

// ============================================================
// PARRY
// ============================================================

void PlayerParry::Enter(Player* player)
{
    player->color = { 0.0f, 0.5f, 1.0f, 1.0f }; // Flash blue
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    player->GetAnimator()->PlayUpper("Parry", false);
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
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    player->SetAimLocked(false);
}

// ============================================================
// SHOOT
// ============================================================

void PlayerShoot::Enter(Player* player)
{
    player->color = { 1.0f, 1.0f, 0.0f, 1.0f }; // Flash yellow
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
    player->color = { 1.0f, 1.0f, 1.0f, 1.0f };
}