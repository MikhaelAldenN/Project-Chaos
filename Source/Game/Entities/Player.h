#pragma once
#include "Bullet.h"
#include "Character.h"
#include "PlayerConstants.h"
#include <deque>
#include <memory>
#include <DirectXMath.h>
#include <characterkinematic/PxController.h>
#include <characterkinematic/PxCapsuleController.h>
#include <characterkinematic/PxControllerManager.h>

class StateMachine;
class AnimationController;
class Camera;

class Player : public Character
{
public:
    Player();
    ~Player() override;

    void Update(float elapsedTime, Camera* camera) override;

    // --- Component accessors (raw pointers, no ownership transfer) ---
    StateMachine* GetStateMachine() const { return stateMachine.get(); }
    CharacterMovement* GetMovement()     const { return movement.get(); }
    AnimationController* GetAnimator()     const { return animator.get(); }
    std::shared_ptr<Model> GetModel()        const { return model; }

    // --- Input & camera ---
    void SetInputEnabled(bool enable) { isInputEnabled = enable; }
    void SetCamera(Camera* cam) { activeCamera = cam; }

    // --- Position helpers ---
    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& pos);

    // --- Movement config ---
    void SetMoveSpeed(float speed) { moveSpeed = speed; }
    void SetInvertControls(bool invert) { invertControls = invert; }
    void SetGravityEnabled(bool enable) { gravityEnabled = enable; }

    // --- Physics init (call once after scene PhysX setup) ---
    // spawnY: initial capsule Y position. SceneGame uses 15.0 (falls to ground).
    //         SceneBoss uses CapsuleHalfHeight (gravity off, no ground plane).
    void InitPhysics(physx::PxControllerManager* manager, physx::PxMaterial* material,
        float spawnY = 15.0f);

    // --- Aim ---
    void RotateModelToPoint(const DirectX::XMFLOAT3& targetPos);

    // --- Projectiles ---
    void FireProjectile();
    void RenderProjectiles(ModelRenderer* renderer);
    std::deque<std::unique_ptr<Bullet>>& GetProjectiles() { return m_projectiles; }

    // --- Debug ---
    void DrawDebugGUI();

    // Returns true when player has meaningful smoothed input
    bool IsMoving() const
    {
        return (std::abs(currentSmoothInput.x) > 0.01f ||
            std::abs(currentSmoothInput.y) > 0.01f);
    }

    // Visual tint (used by states for hit flash, etc.)
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    // --- Read accessors for state machine ---
    float GetBaseSpeed()    const { return baseSpeed; }
    float GetDashSpeed()    const { return dashSpeed; }
    float GetDashDuration() const { return dashDuration; }
    DirectX::XMFLOAT2 GetLastValidInput() const { return lastValidInput; }

    // Written by PlayerDash state
    bool  canDash = true;
    float dashCooldownTimer = 0.0f;

private:
    // --- Update pipeline (called in order from Update()) ---
    void UpdateDashCooldown(float dt);
    void HandleMovementInput(float dt);
    void UpdateHorizontalMovement(float dt);
    void UpdateFootRotation(float dt, float& outSmoothedYaw);
    void UpdateAimConstraint(float& inOutSmoothedYaw, bool& outShouldAim, float& outRelativeAngle);
    void ApplyWorldMatrix(float smoothedYaw, bool shouldAim, float relativeAngle);
    void UpdateProjectiles(float dt, Camera* camera);

    // --- Owned components ---
    std::unique_ptr<StateMachine>        stateMachine;
    std::unique_ptr<AnimationController> animator;

    // --- PhysX controller (lifecycle managed by PhysX, released manually in destructor) ---
    physx::PxController* m_physxController = nullptr;

    // --- Camera ---
    Camera* activeCamera = nullptr;

    // --- Input state ---
    bool isInputEnabled = true;
    bool invertControls = false;
    bool gravityEnabled = true;   // Set false for top-down scenes (e.g. SceneBoss)
    DirectX::XMFLOAT2 currentSmoothInput = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 lastValidInput = { 0.0f, 1.0f };

    // --- Movement params (runtime-tunable, initialized from PlayerConst) ---
    float moveSpeed = PlayerConst::MoveSpeed;
    float acceleration = PlayerConst::Acceleration;
    float deceleration = PlayerConst::Deceleration;

    // --- Dash params ---
    float baseSpeed = 10.0f;
    float dashSpeed = PlayerConst::DashSpeed;
    float dashDuration = PlayerConst::DashDuration;
    float dashCooldown = PlayerConst::DashCooldown;

    // --- Aim target (set by RotateModelToPoint) ---
    DirectX::XMFLOAT3 m_aimTarget = { 0.0f, 0.0f, 0.0f };

    // --- Projectile pool ---
    std::deque<std::unique_ptr<Bullet>> m_projectiles;
};