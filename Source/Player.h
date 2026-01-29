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
    void UpdateHorizontalMovement(float elapsedTime);

    void HandleMovementInput(float elpasedTime);
    //bool CheckJumpInput();
    void SetInputEnabled(bool enable) { isInputEnabled = enable; }
    void SetCamera(Camera* cam) { activeCamera = cam; }

    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& pos);

    // --- Breakout / Mashing System ---
    void SetBreakoutMode(bool enable);

    // Get current energy 
    float GetShakeEnergy() const { return shakeEnergy; }

    // Checkpoint Stages: 0 = Normal, 1 = Yellow (Formation), 2 = Max (Destruction)
    void SetGameStage(int stage) { gameStage = stage; }
    int GetGameStage() const { return gameStage; }

    // --- ESCAPE LOGIC ---
    void TriggerEscape();
    bool IsEscaping() const { return isEscaping; }

    // --- Invicible Logic ---
    void ActivateInvincibility() 
    {
        m_isInvincible = true;
        m_invincibleTimer = invincibleSettings.Duration;
    }

    bool IsInvincible() const { return m_isInvincible; }

    // Settings struct for easy header tuning
    struct InvincibleConfig 
    {
        float Duration = 7.0f;
        DirectX::XMFLOAT4 VisualColor = { 0.275f, 0.275f, 0.275f, 1.0f }; // Stage Color
    };
    InvincibleConfig invincibleSettings;

    void SetInvertControls(bool enable) { invertControls = enable; }

    // --- SETTINGS ---
    struct BreakoutSettings
    {
        // [VISUAL] Screen Shake Effects
        float visualShakeGain = 0.1f;           // Shake intensity added per click
        float visualShakeDecay = 1.2f;          // How fast shake stops
        float visualMaxShake = 0.8f;            // Max displacement

        // [GAMEPLAY] Energy / Mashing Logic    
        float energyGain = 7.2f;                // Energy added per click
        float energyDecay = 40.0f;              // Energy lost per second 
        float energyMax = 100.0f;               // Max bar value

        // [THRESHOLDS] When things happen
        float thresholdFormation = 60.0f;       // Phase 1 (Pale Yellow)
        float thresholdDestruction = 100.0f;    // Phase 2 (White/Destroy)

        // [COLORS] 
        DirectX::XMFLOAT4 colorYellow = { 0.96f, 0.80f, 0.23f, 1.0f };  // Start Color
        DirectX::XMFLOAT4 colorPale = { 1.0f, 0.89f, 0.58f, 1.0f };     // Phase 1 Target
        DirectX::XMFLOAT4 colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };      // Phase 2 Target
    };

    BreakoutSettings breakoutSettings;

    // Color Setting
    DirectX::XMFLOAT4 color = { 0.96f, 0.80f, 0.23f, 1.0f };

    void DrawDebugGUI();

    void SetMovementLock(bool locked) { m_isMovementLocked = locked; }
    bool IsMovementLocked() const { return m_isMovementLocked; }

private:
    void UpdateBreakoutLogic(float elapsedTime);
    void UpdateEscapeLogic(float elapsedTime);

    // --- CONFIGURATION ---
    DirectX::XMFLOAT3 defaultScale = { 3.0f, 3.0f, 3.0f }; // Player Model Scale

    // --- INTERNAL STATE ---
    StateMachine* stateMachine;
    AnimationController* animator;
    Camera* activeCamera = nullptr;
    bool isInputEnabled = true;

    // --- Breakout State ---
    bool isBreakoutActive = false;
    bool isEscaping = false;
    bool m_isInvincible = false;
    bool wasSpacePressed = false;                               
    float currentShakeIntensity = 0.0f;
    float escapeTargetZ = -27.0f;
    float m_invincibleTimer = 0.0f;
    float shakeEnergy = 0.0f; 
    int gameStage = 0;
    DirectX::XMFLOAT3 originalPosition = { 0.0f, 0.0f, 0.0f }; 

	bool invertControls = false;

    // --- MOVEMENT SETTINGS ---
    float moveSpeed = 3.0f;          // Kecepatan maksimal
    float acceleration = 8.0f;      // Seberapa cepat mencapai top speed (Smoothing Start)
    float deceleration = 10.0f;      // Seberapa cepat berhenti (Smoothing Stop)

    // Menyimpan input yang sudah di-smooth (Current Velocity dalam konteks Input)
    DirectX::XMFLOAT2 currentSmoothInput = { 0.0f, 0.0f };

    bool m_isMovementLocked = false;
};