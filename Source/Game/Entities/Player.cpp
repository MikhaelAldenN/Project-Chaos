#include "System/Input.h"
#include "System/Graphics.h"
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerConstants.h"
#include "PlayerStates.h"
#include "StateMachine.h"
#include <cmath>
#include <imgui.h>
#include "InputHelper.h"

using namespace DirectX;

// ============================================================
// LIFECYCLE
// ============================================================

Player::Player()
    : stateMachine(std::make_unique<StateMachine>())
    , animator(std::make_unique<AnimationController>())
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/TEST_mdl_Player3.glb");
    scale = { 1.0f, 1.0f, 1.0f };

	// Load weapons and set their local offsets for correct hand positioning
    m_weapons[static_cast<size_t>(WeaponType::Crossbow)] = std::make_unique<Weapon>(device, "Data/Model/Character/WEAPON_mdl_Crossbow.glb");
    m_weapons[static_cast<size_t>(WeaponType::Crossbow)]->SetLocalOffset(
        { 0.000f, 0.000f, 0.000f },
        { 90.000f, 99.000f, 0.000f },
        { 0.900f, 0.900f, 0.400f }
    );

    m_weapons[static_cast<size_t>(WeaponType::Sword)] = std::make_unique<Weapon>(device, "Data/Model/Character/WEAPON_mdl_Sword.glb");
    m_weapons[static_cast<size_t>(WeaponType::Sword)]->SetLocalOffset(
        { 0.000f, 0.001f, 0.000f },
        { 0.000f, 180.000f, 0.000f },
        { 0.350f, 0.350f, 0.350f }
    );

    if (model) {
        m_rightHandBoneIndex = model->GetNodeIndex("hand.r");
    }

    m_playerbulletModel = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Paddle.glb");

    animator->Initialize(model);
    animator->SetUpperBodyMaskRoot("body");
    stateMachine->Initialize(std::make_unique<PlayerIdle>(), this);

    // Log loaded animations to debug output
    OutputDebugStringA("\n=== ANIMATIONS LOADED ===\n");
    const auto& anims = model->GetAnimations();
    for (size_t i = 0; i < anims.size(); ++i)
    {
        std::string msg = "[" + std::to_string(i) + "] " + anims[i].name + "\n";
        OutputDebugStringA(msg.c_str());
    }
    OutputDebugStringA("=========================\n\n");

    m_capeSimulator = std::make_unique<CapeSimulator>();

    auto GenerateBoneNames = [](const char* prefix, int startIdx, int endIdx)
        {
            std::vector<std::string> names;
            char buffer[32];
            for (int i = startIdx; i <= endIdx; ++i)
            {
                // %03d automatically adds the zeros: 1 becomes "001", 15 becomes "015"
                snprintf(buffer, sizeof(buffer), "%s%03d", prefix, i);
                names.push_back(std::string(buffer));
            }
            return names;
        };

    // Chain 1: Center (001 to 007)
    m_capeSimulator->AddChain(model, GenerateBoneNames("cape.", 1, 7));

    // Chain 2: Right (008 to 015)
    m_capeSimulator->AddChain(model, GenerateBoneNames("cape.", 8, 15));

    // Chain 3: Left (016 to 023)
    m_capeSimulator->AddChain(model, GenerateBoneNames("cape.", 16, 23));

    color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

Player::~Player()
{
    // stateMachine and animator cleaned up by unique_ptr automatically.
    // PhysX controller is not owned by a smart pointer — release manually.
    if (m_physxController) m_physxController->release();
}

void Player::InitPhysics(physx::PxControllerManager* manager, physx::PxMaterial* material, float spawnY)
{
    physx::PxCapsuleControllerDesc desc;
    desc.height = PlayerConst::CapsuleHeight;
    desc.radius = PlayerConst::CapsuleRadius;
    desc.position = physx::PxExtendedVec3(0.0, spawnY, 0.0);
    desc.material = material;
    desc.stepOffset = PlayerConst::CapsuleStep;

    m_physxController = manager->createController(desc);
}

void Player::Update(float elapsedTime, Camera* camera)
{
    if (m_invincibilityTimer > 0.0f)
    {
        m_invincibilityTimer -= elapsedTime;
    }

    UpdateDashCooldown(elapsedTime);

    SetCamera(camera);
    if (isInputEnabled) HandleMovementInput(elapsedTime);
    else currentSmoothInput = { 0.0f, 0.0f };

    UpdateHorizontalMovement(elapsedTime);

    // -------------------------------------------------------------
    // ---> DEBUG: BYPASS THE STATE MACHINE FOR WEAPON TUNING <---
    // -------------------------------------------------------------
    if (m_debugState.forceAnimation)
    {
        // Force the animation on loop, bypassing the State Machine entirely!
        if (!animator->IsPlaying(m_debugState.animationName))
        {
            // Note: We play this on the FULL body, ignoring PlayUpper, 
            // so you get a perfectly clean stance for tuning!
            animator->Play(m_debugState.animationName, true, 0.2f);
        }
    }
    else
    {
        // Normal Gameplay
        if (stateMachine) stateMachine->Update(this, elapsedTime);
    }
    // -------------------------------------------------------------

    if (animator) animator->Update(elapsedTime);

    // -------------------------------------------------------------
    // ---> NEW: THE WEAPON STATE MANAGER (Auto-Sheathe) <---
    // -------------------------------------------------------------
    // If the player is holding the Sword, but the upper-body attack 
    // animation has officially finished, automatically revert to the Crossbow.
    // The !m_debugState check ensures the sword doesn't vanish while you are tuning it in the GUI!
    if (!m_debugState.forceAnimation && m_activeWeaponType == WeaponType::Sword && !animator->IsUpperPlaying())
    {
        SetActiveWeapon(WeaponType::Crossbow);
        m_aimLocked = false;
    }
    // -------------------------------------------------------------

    float smoothedYaw = 0.0f;
    bool  shouldAim = false;
    float relativeAngle = 0.0f;

    UpdateFootRotation(elapsedTime, smoothedYaw);
    UpdateAimConstraint(elapsedTime, smoothedYaw, shouldAim, relativeAngle);

    // ---> DEBUG: DISABLE SPINE TWIST <---
    if (m_debugState.disableAimConstraint)
    {
        shouldAim = false;
        relativeAngle = 0.0f;
    }

    ApplyWorldMatrix(smoothedYaw, shouldAim, relativeAngle);
    UpdateProjectiles(elapsedTime, camera);
}

// ============================================================
// UPDATE SUB-STEPS
// ============================================================

void Player::UpdateDashCooldown(float dt)
{
    if (canDash) return;

    dashCooldownTimer -= dt;
    if (dashCooldownTimer <= 0.0f) canDash = true;
}

void Player::HandleMovementInput(float dt)
{
    float targetX = 0.0f;
    float targetZ = 0.0f;

    if (isInputEnabled) {
        if (GetAsyncKeyState('W') & 0x8000) targetZ = 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) targetZ = -1.0f;
        if (GetAsyncKeyState('A') & 0x8000) targetX = -1.0f;
        if (GetAsyncKeyState('D') & 0x8000) targetX = 1.0f;
    }
    if (invertControls) { targetX = -targetX; targetZ = -targetZ; }

    // Normalize diagonal input
    if (targetX != 0.0f && targetZ != 0.0f)
    {
        float len = std::sqrt(targetX * targetX + targetZ * targetZ);
        targetX /= len;
        targetZ /= len;
    }

    // Smooth acceleration / deceleration
    float smoothX = (targetX != 0.0f) ? acceleration : deceleration;
    float smoothZ = (targetZ != 0.0f) ? acceleration : deceleration;
    currentSmoothInput.x += (targetX - currentSmoothInput.x) * smoothX * dt;
    currentSmoothInput.y += (targetZ - currentSmoothInput.y) * smoothZ * dt;

    // Snap to zero below threshold to avoid float drift
    if (std::abs(currentSmoothInput.x) < 0.01f) currentSmoothInput.x = 0.0f;
    if (std::abs(currentSmoothInput.y) < 0.01f) currentSmoothInput.y = 0.0f;

    // Track last non-zero input direction (used by dash for launch direction)
    if (targetX != 0.0f || targetZ != 0.0f)
        lastValidInput = { targetX, targetZ };
}

void Player::UpdateHorizontalMovement(float dt)
{
    float displacementX = 0.0f;
    float displacementZ = 0.0f;

    XMFLOAT3 stateVelocity = movement->GetVelocity();

    // State-driven velocity (e.g. dash) takes priority over walk input
    if (stateVelocity.x != 0.0f || stateVelocity.z != 0.0f)
    {
        displacementX = stateVelocity.x * dt;
        displacementZ = stateVelocity.z * dt;
    }
    else
    {
        displacementX = currentSmoothInput.x * moveSpeed * dt;
        displacementZ = currentSmoothInput.y * moveSpeed * dt;
    }

    // Apply displacement through PhysX capsule controller
    const float gravityY = gravityEnabled ? PlayerConst::Gravity * dt : 0.0f;
    physx::PxVec3 displacement(displacementX, gravityY, displacementZ);
    m_physxController->move(displacement, PlayerConst::PhysXMinDist, dt, physx::PxControllerFilters());

    // Mirror PhysX position back to our movement state (offset by capsule half-height)
    physx::PxExtendedVec3 pxPos = m_physxController->getPosition();
    movement->SetPosition({ (float)pxPos.x, (float)pxPos.y - PlayerConst::CapsuleHalfHeight, (float)pxPos.z });
}

void Player::UpdateFootRotation(float dt, float& outSmoothedYaw)
{
    XMFLOAT2 moveInput = GetLastValidInput();
    float currentYaw = XMConvertToRadians(movement->GetRotation().y);
    float targetYaw = currentYaw;

    if (moveInput.x != 0.0f || moveInput.y != 0.0f)
        targetYaw = atan2f(moveInput.x, moveInput.y);

    m_isBackpedaling = false;

    XMFLOAT3 pos = movement->GetPosition();
    float dx = m_aimTarget.x - pos.x;
    float dz = m_aimTarget.z - pos.z;

    if ((dx * dx + dz * dz) > PlayerConst::AimMinDistSq)
    {
        float aimYaw = atan2f(dx, dz);
        float diff = targetYaw - aimYaw;

        while (diff > XM_PI) diff -= XM_2PI;
        while (diff < -XM_PI) diff += XM_2PI;

        if (diff > XM_PIDIV2)
        {
            diff = XM_PI - diff;
            targetYaw = aimYaw + diff;
            m_isBackpedaling = true; // <-- NEW: We are backpedaling!
        }
        else if (diff < -XM_PIDIV2)
        {
            diff = -XM_PI - diff;
            targetYaw = aimYaw + diff;
            m_isBackpedaling = true; // <-- NEW: We are backpedaling!
        }

        while (targetYaw > XM_PI) targetYaw -= XM_2PI;
        while (targetYaw < -XM_PI) targetYaw += XM_2PI;
    }

    float angleDiff = targetYaw - currentYaw;
    while (angleDiff > XM_PI) angleDiff -= XM_2PI;
    while (angleDiff < -XM_PI) angleDiff += XM_2PI;

    float lerpFactor = min(PlayerConst::RotSmoothSpeed * dt, 1.0f);
    outSmoothedYaw = currentYaw + angleDiff * lerpFactor;
}

void Player::UpdateAimConstraint(float dt, float& inOutSmoothedYaw, bool& outShouldAim, float& outRelativeAngle)
{
    outShouldAim = false;
    outRelativeAngle = 0.0f;

    if (!model || !activeCamera) return;

    DirectX::XMFLOAT3 pos = movement->GetPosition();
    float dx = m_aimTarget.x - pos.x;
    float dz = m_aimTarget.z - pos.z;

    if ((dx * dx + dz * dz) <= PlayerConst::AimMinDistSq) return;

    outShouldAim = true;

    float absoluteAngleToMouse = atan2f(dx, dz);
    float relativeAngle = absoluteAngleToMouse - inOutSmoothedYaw;

    while (relativeAngle > DirectX::XM_PI) relativeAngle -= DirectX::XM_2PI;
    while (relativeAngle < -DirectX::XM_PI) relativeAngle += DirectX::XM_2PI;

    // -----------------------------------------------------------------
    // ---> THE FIX: SMOOTH FOOT DRAG (Zero Duplication Math) <---
    // -----------------------------------------------------------------
    // Clamp torso to ±MaxTorsoAngle; if clamped, PULL feet smoothly to compensate
    if (std::abs(relativeAngle) > PlayerConst::MaxTorsoAngle)
    {
        // 1. Get the direction of the twist (1.0f for right, -1.0f for left)
        float sign = (relativeAngle > 0.0f) ? 1.0f : -1.0f;

        // 2. Safely clamp the spine twist
        relativeAngle = PlayerConst::MaxTorsoAngle * sign;

        // 3. Calculate exactly where the feet NEED to be to support this spine twist
        float targetFootYaw = absoluteAngleToMouse - relativeAngle;

        // 4. Find the shortest path for the feet to rotate
        float diff = targetFootYaw - inOutSmoothedYaw;
        while (diff > DirectX::XM_PI) diff -= DirectX::XM_2PI;
        while (diff < -DirectX::XM_PI) diff += DirectX::XM_2PI;

        // 5. Smoothly drag the feet over time instead of teleporting them!
        inOutSmoothedYaw += diff * (std::min)(PlayerConst::RotSmoothSpeed * dt, 1.0f);
    }

    outRelativeAngle = relativeAngle;
}

void Player::ApplyWorldMatrix(float smoothedYaw, bool shouldAim, float relativeAngle)
{
    // Commit smoothed foot yaw to movement state
    movement->SetRotationY(XMConvertToDegrees(smoothedYaw));

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z));
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    // Inject torso bone twist for upper-body aiming
    if (shouldAim && model)
    {
        int bodyIndex = model->GetNodeIndex("body");
        if (bodyIndex != -1)
        {
            Model::Node& bodyNode = model->GetNodes()[bodyIndex];

            // Get the Parent's (Hips/Pelvis) Global Matrix
            XMMATRIX parentGlobal = XMMatrixIdentity();
            if (bodyNode.parent != nullptr) {
                parentGlobal = XMLoadFloat4x4(&bodyNode.parent->globalTransform);
            }

            // Find the "True Sky" inside the tilted Hip space
            XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            XMMATRIX parentInverse = XMMatrixInverse(nullptr, parentGlobal);
            XMVECTOR localUpAxis = XMVector3TransformNormal(worldUp, parentInverse);
            localUpAxis = XMVector3Normalize(localUpAxis);

            // Create a rotation around that specific calculated axis 
            XMMATRIX twistMatrix = XMMatrixRotationAxis(localUpAxis, relativeAngle);

            // Apply it to the animation
            XMVECTOR currentLocalRot = XMLoadFloat4(&bodyNode.rotation);
            XMMATRIX localMatrix = XMMatrixRotationQuaternion(currentLocalRot);

            // Multiply Local * Twist
            XMVECTOR finalRot = XMQuaternionRotationMatrix(localMatrix * twistMatrix);
            XMStoreFloat4(&bodyNode.rotation, finalRot);
        }
    }

    if (m_capeSimulator)
    {
        DirectX::XMFLOAT3 trueVelocity = movement->GetVelocity();

        if (trueVelocity.x == 0.0f && trueVelocity.z == 0.0f)
        {
            trueVelocity.x = currentSmoothInput.x * moveSpeed;
            trueVelocity.z = currentSmoothInput.y * moveSpeed;
        }

        m_capeSimulator->Update(0.016f, trueVelocity);
    }

    if (model) model->UpdateTransform(worldMatrix);

    DirectX::XMFLOAT4X4 attachMatrix = worldMatrix; // Fallback to feet if hand is missing
    if (m_rightHandBoneIndex != -1 && model->GetNodes().size() > m_rightHandBoneIndex)
    {
        attachMatrix = model->GetNodes()[m_rightHandBoneIndex].worldTransform;
    }

    // Optimization: Range-based for-loop. Updates all weapons so they are ready instantly.
    for (const auto& weapon : m_weapons)
    {
        if (weapon) weapon->UpdateTransform(attachMatrix);
    }
}

void Player::UpdateProjectiles(float dt, Camera* camera)
{
    DirectX::XMFLOAT3 myPos = movement->GetPosition();

    // Max distance before the bullet vanishes
    constexpr float DESPAWN_DISTANCE = 55.0f;
    constexpr float DESPAWN_DIST_SQ = DESPAWN_DISTANCE * DESPAWN_DISTANCE;

    for (auto& bullet : m_projectiles)
    {
        if (!bullet->IsActive()) continue;

        bullet->Update(dt, camera);

        // ---> BUG PREVENTION: The Infinite Flight Guard <---
        DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
        float dx = myPos.x - bPos.x;
        float dz = myPos.z - bPos.z;

        if ((dx * dx + dz * dz) > DESPAWN_DIST_SQ)
        {
            // The bullet missed and flew off-screen.
            // DO NOT ERASE IT! Turn it off so we can recycle its memory later!
            bullet->SetActive(false);
        }
    }
}

// ============================================================
// AIM & PROJECTILES
// ============================================================

void Player::RotateModelToPoint(const DirectX::XMFLOAT3& targetPos)
{
    if (!m_aimLocked) {
        m_aimTarget = targetPos;
    }
}

void Player::FireProjectile()
{
    if (!isInputEnabled) return;

    DirectX::XMFLOAT3 myPos = movement->GetPosition();

    float dx = m_aimTarget.x - myPos.x;
    float dz = m_aimTarget.z - myPos.z;
    float angleToMouse = atan2f(dx, dz);
    DirectX::XMFLOAT3 fwd = { sinf(angleToMouse), 0.0f, cosf(angleToMouse) };

    // Spawn slightly ahead of the player at chest height
    DirectX::XMFLOAT3 spawnPos =
    {
        myPos.x + fwd.x * PlayerConst::BulletSpawnFwd,
        myPos.y + PlayerConst::BulletSpawnY,
        myPos.z + fwd.z * PlayerConst::BulletSpawnFwd
    };

    // --------------------------------------------------------
    // ---> BUG PREVENTION: THE TRUE OBJECT POOL <---
    // --------------------------------------------------------

    // Search our pool for an inactive (dead/invisible) bullet
    for (auto& bullet : m_projectiles)
    {
        if (!bullet->IsActive())
        {
            // RECYCLE IT! 
            bullet->Fire(spawnPos, fwd, m_bulletSpeed);

            // EARLY EXIT: We saved the CPU from allocating new memory!
            return;
        }
    }

    // If we get here, it means EVERY bullet we own is currently flying on-screen.
    // ONLY THEN do we allocate new memory.
    auto newBullet = std::make_unique<Bullet>();
    newBullet->Fire(spawnPos, fwd, PlayerConst::BulletSpeed);
    m_projectiles.push_back(std::move(newBullet));

    // Prevent memory leaks. If the pool gets ridiculously large, pop the oldest.
    for (int i = 0; i < PlayerConst::MaxBullets; ++i) {
        auto b = std::make_unique<Bullet>();
        b->SetActive(false);
        m_projectiles.push_back(std::move(b));
    }
}

void Player::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
    {
        if (!bullet->IsActive()) continue;

        if (m_playerbulletModel)
        {
            DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
            DirectX::XMFLOAT3 bVel = bullet->GetVelocity();
            float yaw = atan2f(bVel.x, bVel.z);

            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(
                m_playerbulletOffsetScale.x,
                m_playerbulletOffsetScale.y,
                m_playerbulletOffsetScale.z
            );

            DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(
                DirectX::XMConvertToRadians(m_playerbulletOffsetRot.x),
                DirectX::XMConvertToRadians(m_playerbulletOffsetRot.y),
                DirectX::XMConvertToRadians(m_playerbulletOffsetRot.z)
            );
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(m_playerbulletOffsetPos.x, m_playerbulletOffsetPos.y, m_playerbulletOffsetPos.z);

            DirectX::XMMATRIX bulletRot = DirectX::XMMatrixRotationY(yaw);
            DirectX::XMMATRIX bulletTrans = DirectX::XMMatrixTranslation(bPos.x, bPos.y, bPos.z);

            DirectX::XMFLOAT4X4 worldMatrix;
            DirectX::XMStoreFloat4x4(&worldMatrix, S * R * T * bulletRot * bulletTrans);

            // Draw ONCE using standard Phong! The PostProcessor will see the HDR color and bloom it automatically!
            renderer->Draw(ShaderId::Basic, m_playerbulletModel, m_playerbulletColor, worldMatrix);
        }
        else
        {
            renderer->Draw(ShaderId::Phong, bullet->GetModel(), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
}

void Player::RenderWeapon(ModelRenderer* renderer)
{
    if (Weapon* activeWpn = GetActiveWeapon())
    {
        activeWpn->Render(renderer);
    }
}

// ============================================================
// DAMAGE SYSTEM
// ============================================================
void Player::TakeDamage(int damage)
{
    if (m_hp <= 0 || IsInvincible()) return;

    m_hp -= damage;

    // [修正] トグルがONの時だけ無敵時間（指定秒数）を付与する
    if (m_enableIFrames) {
        TriggerInvincibility(m_iFrameDuration);
    }

    if (m_hp <= 0)
    {
        m_hp = 0;
    }
}

// ============================================================
// HELPERS
// ============================================================

void Player::SetPosition(float x, float y, float z)
{
    if (movement) movement->SetPosition({ x, y, z });
}

void Player::SetPosition(const DirectX::XMFLOAT3& pos)
{
    // Update komponen movement logical
    if (movement) movement->SetPosition(pos);

    // PENTING: Update juga komponen fisika PhysX
    if (m_physxController)
    {
        // PxExtendedVec3 membutuhkan offset Y karena posisi 'movement' 
        // berada di dasar kaki, sedangkan PxController (kapsul) 
        // posisinya dihitung dari tengah kapsul (titik pusat/centroid).
        m_physxController->setPosition(physx::PxExtendedVec3(
            pos.x,
            pos.y + PlayerConst::CapsuleHalfHeight,
            pos.z
        ));
    }
}

void Player::ReleasePowerCap()
{
    if (m_isPowerUncapped) return;
    m_isPowerUncapped = true;

    // Simpan nilai saat ini sebelum ditimpa, agar bisa dikembalikan nanti
    m_normalMoveSpeed = moveSpeed;
    m_normalDashSpeed = dashSpeed;
    m_normalColor = color;

    // Terapkan atribut Overdrive
    moveSpeed = m_uncapMoveSpeed;
    dashSpeed = m_uncapDashSpeed;
    color = m_uncapColor;
}

void Player::RestorePowerCap()
{
    if (!m_isPowerUncapped) return;
    m_isPowerUncapped = false;

    // Kembalikan atribut ke nilai normal
    moveSpeed = m_normalMoveSpeed;
    dashSpeed = m_normalDashSpeed;
    color = m_normalColor;
}


void Player::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Movement & Physics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Status: %s", isInputEnabled ? "Input ON" : "Input OFF");
        ImGui::Checkbox("Invert Controls", &invertControls);
        ImGui::DragFloat("Walk Speed", &moveSpeed, 0.1f, 0.0f, 100.0f, "%.1f");
        ImGui::DragFloat("Acceleration", &acceleration, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Deceleration", &deceleration, 0.1f, 0.1f, 100.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Dash Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Dash Speed", &dashSpeed, 0.5f, 10.0f, 200.0f, "%.1f");
        ImGui::DragFloat("Dash Duration", &dashDuration, 0.01f, 0.01f, 1.0f, "%.2f sec");
        ImGui::DragFloat("Dash Cooldown", &dashCooldown, 0.01f, 0.0f, 5.0f, "%.2f sec");
    }

    if (ImGui::CollapsingHeader("Combat & Projectiles", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[ General Combat ]");
        int hp = GetHP();
        if (ImGui::InputInt("Player HP", &hp)) SetMaxHP(hp);

        // --- 無敵時間 (I-Frames) のコントロール ---
        ImGui::Checkbox("Enable I-Frames (Invincibility on hit)", &m_enableIFrames);
        if (m_enableIFrames) {
            ImGui::Indent();
            ImGui::DragFloat("I-Frame Duration", &m_iFrameDuration, 0.1f, 0.1f, 5.0f, "%.1f sec");
            ImGui::Unindent();
        }
        ImGui::Separator();

        // --- Toggle Uncap (Overdrive) ---
        bool powerUncapped = IsPowerUncapped();
        if (ImGui::Checkbox("Uncap Power (Overdrive)", &powerUncapped)) {
            if (powerUncapped) ReleasePowerCap();
            else RestorePowerCap();
        }

        // --- Parameter Uncap Muncul Jika Aktif ---
        if (powerUncapped) {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), ">> Uncap Tuning <<");

            // Jika slider digeser saat Uncap aktif, langsung terapkan nilainya secara real-time
            if (ImGui::DragFloat("Uncap Walk Speed", &m_uncapMoveSpeed, 0.1f, 10.0f, 100.0f, "%.1f")) moveSpeed = m_uncapMoveSpeed;
            if (ImGui::DragFloat("Uncap Dash Speed", &m_uncapDashSpeed, 0.5f, 10.0f, 200.0f, "%.1f")) dashSpeed = m_uncapDashSpeed;
            if (ImGui::ColorEdit4("Uncap Glow Color", (float*)&m_uncapColor)) color = m_uncapColor;

            ImGui::Unindent();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ Crossbow Bullet ]");
        ImGui::DragFloat("Bullet Speed", &m_bulletSpeed, 0.5f, 1.0f, 150.0f, "%.1f");
        ImGui::ColorEdit4("Bullet Tint Color", (float*)&m_playerbulletColor);

        if (ImGui::TreeNode("Bullet Model Transform (Offset)"))
        {
            ImGui::DragFloat3("Position", (float*)&m_playerbulletOffsetPos, 0.01f);
            ImGui::DragFloat3("Rotation", (float*)&m_playerbulletOffsetRot, 0.5f);
            ImGui::DragFloat3("Scale", (float*)&m_playerbulletOffsetScale, 0.1f);
            if (ImGui::Button("Reset Offsets", ImVec2(-1.0f, 25.0f))) ResetPlayerBulletOffsets();
            ImGui::TreePop();
        }
    }
}