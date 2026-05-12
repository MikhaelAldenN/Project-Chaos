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

    m_equippedWeapon = std::make_unique<Weapon>(device, "Data/Model/Character/WEAPON_mdl_Crossbow.glb");
    m_equippedWeapon->SetLocalOffset(
        { 0.000f, 0.000f, 0.000f },   // Position
        { 90.000f, 0.000f, 0.000f },  // Rotation
        { 0.400f, 0.400f, 0.400f }    // Scale
    );
    if (model) {
        m_rightHandBoneIndex = model->GetNodeIndex("hand.r");
    }

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

// ============================================================
// UPDATE — orchestrator only, no inline logic
// ============================================================

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

    if (stateMachine) stateMachine->Update(this, elapsedTime);
    if (animator)     animator->Update(elapsedTime);

    float smoothedYaw = 0.0f;
    bool  shouldAim = false;
    float relativeAngle = 0.0f;

    UpdateFootRotation(elapsedTime, smoothedYaw);
    UpdateAimConstraint(smoothedYaw, shouldAim, relativeAngle);
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

    if (GetAsyncKeyState('W') & 0x8000) targetZ = 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) targetZ = -1.0f;
    if (GetAsyncKeyState('A') & 0x8000) targetX = -1.0f;
    if (GetAsyncKeyState('D') & 0x8000) targetX = 1.0f;

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

    // Shortest-path angle delta to avoid wrap-around snapping
    float angleDiff = targetYaw - currentYaw;
    while (angleDiff > XM_PI) angleDiff -= XM_2PI;
    while (angleDiff < -XM_PI) angleDiff += XM_2PI;

    float lerpFactor = min(PlayerConst::RotSmoothSpeed * dt, 1.0f);
    outSmoothedYaw = currentYaw + angleDiff * lerpFactor;
}

void Player::UpdateAimConstraint(float& inOutSmoothedYaw, bool& outShouldAim, float& outRelativeAngle)
{
    outShouldAim = false;
    outRelativeAngle = 0.0f;

    if (!model || !activeCamera) return;

    XMFLOAT3 pos = movement->GetPosition();
    float dx = m_aimTarget.x - pos.x;
    float dz = m_aimTarget.z - pos.z;

    // Skip if aim target is too close (avoids atan2 instability)
    if ((dx * dx + dz * dz) <= PlayerConst::AimMinDistSq) return;

    outShouldAim = true;

    float absoluteAngleToMouse = atan2f(dx, dz);
    float relativeAngle = absoluteAngleToMouse - inOutSmoothedYaw;

    while (relativeAngle > XM_PI) relativeAngle -= XM_2PI;
    while (relativeAngle < -XM_PI) relativeAngle += XM_2PI;

    // Clamp torso to ±MaxTorsoAngle; if clamped, pull feet to compensate
    if (relativeAngle > PlayerConst::MaxTorsoAngle)
    {
        relativeAngle = PlayerConst::MaxTorsoAngle;
        inOutSmoothedYaw = absoluteAngleToMouse - relativeAngle;
    }
    else if (relativeAngle < -PlayerConst::MaxTorsoAngle)
    {
        relativeAngle = -PlayerConst::MaxTorsoAngle;
        inOutSmoothedYaw = absoluteAngleToMouse - relativeAngle;
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

    if (m_equippedWeapon)
    {
        // ---> BUG PREVENTION: The Null Bone Guard <---
        if (m_rightHandBoneIndex != -1 && model->GetNodes().size() > m_rightHandBoneIndex)
        {
            // Extract the perfectly calculated world matrix of the hand
            DirectX::XMFLOAT4X4 handMatrix = model->GetNodes()[m_rightHandBoneIndex].worldTransform;
            m_equippedWeapon->UpdateTransform(handMatrix);
        }
        else
        {
            // Fallback: If the bone is missing, attach it to the player's root feet so it doesn't crash
            m_equippedWeapon->UpdateTransform(worldMatrix);
        }
    }

    if (model) model->UpdateTransform(worldMatrix);
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
            bullet->Fire(spawnPos, fwd, PlayerConst::BulletSpeed);

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
    if ((int)m_projectiles.size() > PlayerConst::MaxBullets)
    {
        m_projectiles.pop_front();
    }
}

void Player::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
        if (bullet->IsActive())
            renderer->Draw(ShaderId::Phong, bullet->GetModel(), { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Player::RenderWeapon(ModelRenderer* renderer)
{
    if (m_equippedWeapon) m_equippedWeapon->Render(renderer);
}

// ============================================================
// DAMAGE SYSTEM
// ============================================================
void Player::TakeDamage(int damage)
{
    // EARLY EXIT: Prevent "Ghost Hits". 
    if (m_hp <= 0) return;

    m_hp -= damage;

    // Optional: You can change the player->color here to Flash Red when hit!

    // Clamp to exactly zero to prevent negative UI bugs
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

void Player::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Player Movement Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Status: %s", isInputEnabled ? "Input ON" : "Input OFF");
        ImGui::DragFloat("Max Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Acceleration", &acceleration, 0.1f, 0.1f, 100.0f);
        ImGui::DragFloat("Deceleration", &deceleration, 0.1f, 0.1f, 100.0f);
        ImGui::Checkbox("Invert Controls", &invertControls);
    }
}