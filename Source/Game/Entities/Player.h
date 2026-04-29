#pragma once

#include "Bullet.h"
#include "Character.h"
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

    StateMachine* GetStateMachine() const { return stateMachine; }
    CharacterMovement* GetMovement() const { return movement; }
    AnimationController* GetAnimator() const { return animator; }
    std::shared_ptr<Model> GetModel() const { return model; }

    void HandleMovementInput(float dt);
    void UpdateHorizontalMovement(float elapsedTime);

    void SetInputEnabled(bool enable) { isInputEnabled = enable; }
    void SetCamera(Camera* cam) { activeCamera = cam; }

    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& pos);

    struct MovementSettings {
        static constexpr float DefaultSpeed = 20.0f;
    };

    void SetMoveSpeed(float speed) { moveSpeed = speed; }
    void SetInvertControls(bool invert) { invertControls = invert; }

    void RotateModelToPoint(const DirectX::XMFLOAT3& targetPos);

    void DrawDebugGUI();

    void InitPhysics(physx::PxControllerManager* manager, physx::PxMaterial* material);

    DirectX::XMFLOAT4 color;

private:
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f };
    StateMachine* stateMachine;
    AnimationController* animator;
    Camera* activeCamera = nullptr;
    bool isInputEnabled = true;
    bool invertControls = false;
    float moveSpeed = MovementSettings::DefaultSpeed;
    float acceleration = 8.0f;
    float deceleration = 12.0f;
    DirectX::XMFLOAT2 currentSmoothInput = { 0.0f, 0.0f };
    physx::PxController* m_physxController = nullptr;

public:
    // Accessors untuk State Machine
    float GetBaseSpeed() const { return baseSpeed; }
    float GetDashSpeed() const { return dashSpeed; }
    float GetDashDuration() const { return dashDuration; }
    DirectX::XMFLOAT2 GetLastValidInput() const { return lastValidInput; }

    // Dash Status
    bool canDash = true;
    float dashCooldownTimer = 0.0f;

private:
    float baseSpeed = 5.0f;      // Kecepatan jalan normal
    float dashSpeed = 60.0f;      // Daya dorong instan dash
    float dashDuration = 0.15f;   // Berapa lama dash berlangsung (detik)
    float dashCooldown = 0.5f;    // Jeda sebelum bisa dash lagi

    // Menyimpan arah terakhir ditekan agar bisa dash meski diam
    DirectX::XMFLOAT2 lastValidInput = { 0.0f, 1.0f };

public:
    void FireProjectile();
    void RenderProjectiles(ModelRenderer* renderer);
    std::deque<std::unique_ptr<Bullet>>& GetProjectiles() { return m_projectiles; }

private:
    std::deque<std::unique_ptr<Bullet>> m_projectiles;
};