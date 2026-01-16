#pragma once

#include "Ball.h"
#include "BlockManager.h"
#include "Character.h"

class Ball;

class Paddle : public Character
{
public:
    Paddle();
    ~Paddle() override;

    void Update(float elapsedTime, Camera* camera) override;
    void UpdateAI(float elapsedTime, Ball* ball, BlockManager* blockManager);
    void CheckCollision(Ball* ball);
    void SetAIEnabled(bool enable) { isAIEnabled = enable; }
    void SetActive(bool active) { isActive = active; }
    bool IsActive() const { return isActive; }

    CharacterMovement* GetMovement() const { return movement; }

    // Allow the scene to access the model for rendering
    std::shared_ptr<Model> GetModel() const { return model; }

    // Color Setting
    DirectX::XMFLOAT4 color = { 0.0f, 0.0f, 0.8f, 1.0f };

private:
    void HandleInput();
    // Helper for random float generation 
    float GetRandomFloat(float range)
    {
        return ((float)(rand() % 100) / 100.0f) * (2.0f * range) - range;
    }

    // ----------------------------------------------------
    // MOVEMENT & POSITION SETTINGS
    // ----------------------------------------------------
    float paddleSpeed = 1.5f;
    float startX = -0.5f;        // Initial spawn X position
    float fixedZ = -4.0f;        // The fixed depth line where paddle stays
    float xLimitLeft = -8.8f;    // Maximum Left position 
    float xLimitRight = 8.8f;    // Maximum Right position

    // ----------------------------------------------------
    // PHYSICS & COLLISION SETTINGS
    // ----------------------------------------------------
    // Adjust these to match your 3D model size
    float paddleHalfWidth = 0.8f;
    float paddleHalfDepth = 0.1f;

    // Controls how wide the ball bounces when hitting the edge
    float maxBounceAngle = 1.3f;

    // ----------------------------------------------------
    // AI SETTINGS
    // ----------------------------------------------------
    float aiLaunchDelay = 1.0f;     // How long AI waits before serving ball
    float aiLaunchSpeed = 8.0f;     // Speed of ball when AI serves
    float aiLaunchRandomX = 1.0f;   // Random X variance on serve (+/-)

    // Prediction & Targeting
    int   aiPredictionSteps = 10;   // Safety limit for wall bounce loops
    float aiMoveTolerance = 0.2f;   // Deadzone to stop jittering

    // Idle / Patrol Behavior
    float aiIdlePatrolRange = 3.0f; // +/- X range to move when idling
    float aiIdleBaseWait = 0.5f;    // Minimum time to wait at a spot
    float aiIdleRandomWait = 1.0f;  // Additional random wait time

    // ----------------------------------------------------
    // INTERNAL STATE
    // ----------------------------------------------------
    float launchTimer = 0.0f;
    bool isAIEnabled = false;
    bool isActive = true;
};