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

void Player::InitPhysics(physx::PxControllerManager* manager, physx::PxMaterial* material)
{
    physx::PxCapsuleControllerDesc desc;
    desc.height = PlayerConst::CapsuleHeight;
    desc.radius = PlayerConst::CapsuleRadius;
    desc.position = physx::PxExtendedVec3(0.0, 15.0, 0.0);
    desc.material = material;
    desc.stepOffset = PlayerConst::CapsuleStep;

    m_physxController = manager->createController(desc);
}

// ============================================================
// UPDATE — orchestrator only, no inline logic
// ============================================================

void Player::Update(float elapsedTime, Camera* camera)
{
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
    const float gravityY = PlayerConst::Gravity * dt;
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
            XMVECTOR currentLocalRot = XMLoadFloat4(&bodyNode.rotation);
            XMMATRIX localMatrix = XMMatrixRotationQuaternion(currentLocalRot);
            XMMATRIX twistMatrix = XMMatrixRotationY(relativeAngle);
            XMVECTOR finalRot = XMQuaternionRotationMatrix(twistMatrix * localMatrix);
            XMStoreFloat4(&bodyNode.rotation, finalRot);
        }
    }

    if (model) model->UpdateTransform(worldMatrix);
}

void Player::UpdateProjectiles(float dt, Camera* camera)
{
    for (auto& bullet : m_projectiles)
        if (bullet->IsActive()) bullet->Update(dt, camera);
}

// ============================================================
// AIM & PROJECTILES
// ============================================================

void Player::RotateModelToPoint(const DirectX::XMFLOAT3& targetPos)
{
    m_aimTarget = targetPos;
}

void Player::FireProjectile()
{
    XMFLOAT3 myPos = movement->GetPosition();

    float dx = m_aimTarget.x - myPos.x;
    float dz = m_aimTarget.z - myPos.z;
    float angleToMouse = atan2f(dx, dz);
    XMFLOAT3 fwd = { sinf(angleToMouse), 0.0f, cosf(angleToMouse) };

    // Spawn slightly ahead of the player at chest height
    XMFLOAT3 spawnPos =
    {
        myPos.x + fwd.x * PlayerConst::BulletSpawnFwd,
        myPos.y + PlayerConst::BulletSpawnY,
        myPos.z + fwd.z * PlayerConst::BulletSpawnFwd
    };

    auto newBullet = std::make_unique<Bullet>();
    newBullet->Fire(spawnPos, fwd, PlayerConst::BulletSpeed);
    m_projectiles.push_back(std::move(newBullet));

    // Cap pool size to prevent unbounded growth
    if ((int)m_projectiles.size() > PlayerConst::MaxBullets) m_projectiles.pop_front();
}

void Player::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
        if (bullet->IsActive())
            renderer->Draw(ShaderId::Phong, bullet->GetModel(), { 1.0f, 1.0f, 1.0f, 1.0f });
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
    if (movement) movement->SetPosition(pos);
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