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

    // --- Breakout / Mashing System ---
    void SetBreakoutMode(bool enable);

    // Get current energy 
    float GetShakeEnergy() const { return shakeEnergy; }

    // Checkpoint Stages: 0 = Normal, 1 = Yellow (Formation), 2 = Max (Destruction)
    void SetGameStage(int stage) { gameStage = stage; }

    // --- SETTINGS ---
    struct BreakoutSettings
    {
        // [VISUAL] Screen Shake Effects
        float visualShakeGain = 0.1f;           // Shake intensity added per click
        float visualShakeDecay = 1.2f;          // How fast shake stops
        float visualMaxShake = 0.8f;            // Max displacement

        // [GAMEPLAY] Energy / Mashing Logic    
        float energyGain = 6.4f;                // Energy added per click
        float energyDecay = 40.0f;              // Energy lost per second 
        float energyMax = 100.0f;               // Max bar value

        // [THRESHOLDS] When things happen
        float thresholdFormation = 60.0f;       // Phase 1 (Yellow)
        float thresholdDestruction = 100.0f;    // Phase 2 (Pale Yellow/Destroy)
    };

    BreakoutSettings breakoutSettings;

    // Color Setting
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

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
    float shakeEnergy = 0.0f; 
    int gameStage = 0;
    DirectX::XMFLOAT3 originalPosition = { 0.0f, 0.0f, 0.0f }; 
};