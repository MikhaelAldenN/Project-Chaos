#pragma once
#include "Character.h"
#include <memory>
#include <DirectXMath.h>

// Forward Declaration
class StateMachine;
class AnimationController;
class Camera;

class Player : public Character
{
public:
    Player();
    ~Player() override;

    void Update(float elapsedTime, Camera* camera) override;

    // --- Public Helpers untuk State ---
    StateMachine* GetStateMachine() const { return stateMachine; }
    CharacterMovement* GetMovement() const { return movement; }
    AnimationController* GetAnimator() const { return animator; }
    std::shared_ptr<Model> GetModel() const { return model; }

    // --- Input & Control ---
    void HandleMovementInput();
    bool CheckJumpInput();
    void SetInputEnabled(bool enable) { isInputEnabled = enable; }
    void SetCamera(Camera* cam) { activeCamera = cam; }

    // --- Breakout Mechanic ---
    void SetBreakoutMode(bool enable);

    struct BreakoutSettings
    {
        float shakeGain = 0.1f;        // How much shake is added per SPACE press
        float shakeDecay = 1.2f;       // How fast the shake calms down per second
        float maxShake = 0.8f;         // The maximum violence of the shake
        float shakeFrequency = 20.0f;  // Speed of vibration
    };

    BreakoutSettings breakoutSettings;

private:
    void UpdateBreakoutLogic(float elapsedTime);

    // --- CONFIGURATION ---
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Player Model Scale

    // --- INTERNAL STATE ---
    StateMachine* stateMachine;
    AnimationController* animator;
    Camera* activeCamera = nullptr;
    bool isInputEnabled = true;

    // --- Breakout State ---
    bool isBreakoutActive = false;
    bool wasSpacePressed = false;                               
    float currentShakeIntensity = 0.0f;
    DirectX::XMFLOAT3 originalPosition = { 0.0f, 0.0f, 0.0f }; 
};