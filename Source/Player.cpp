#include "System/Input.h"
#include "System/Graphics.h" 
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerStates.h" 
#include "StateMachine.h"
#include "JuiceEngine.h"
#include <cmath>


using namespace DirectX;

Player::Player()
{
    // 1. Init Model
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");
    scale = defaultScale;

    // 2. Init Animation Controller
    animator = new AnimationController();
    animator->Initialize(model);

    // 3. Init State Machine
    stateMachine = new StateMachine();
    stateMachine->Initialize(new PlayerIdle(), this);

    color = breakoutSettings.colorYellow;
}

Player::~Player()
{
    if (stateMachine) delete stateMachine;
    if (animator) delete animator;
}

void Player::Update(float elapsedTime, Camera* camera)
{
    SetCamera(camera);

    if (isEscaping)
    {
        UpdateEscapeLogic(elapsedTime);
    }

    else if (isInputEnabled)
    {
        if (stateMachine) stateMachine->Update(this, elapsedTime);
        HandleMovementInput();
    }

    movement->Update(elapsedTime);

    if (isEscaping)
    {
        originalPosition = movement->GetPosition();
    }

    if (isBreakoutActive)
    {
        UpdateBreakoutLogic(elapsedTime);
    }

    if (animator) animator->Update(elapsedTime);
    SyncData();
}

void Player::SetBreakoutMode(bool enable)
{
    isBreakoutActive = enable;

    if (enable)
    {
        originalPosition = movement->GetPosition();
        currentShakeIntensity = 0.0f; 
        color = breakoutSettings.colorYellow;
    }
    else
    {
        movement->SetPosition(originalPosition);
    }
}

void Player::TriggerEscape()
{
    isEscaping = true;
    isInputEnabled = false; 
}

void Player::UpdateBreakoutLogic(float elapsedTime)
{
    bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    // --------------------------------------------------------
    // INPUT HANDLING 
    // --------------------------------------------------------
    if (isSpaceDown && !wasSpacePressed)
    {
        currentShakeIntensity += breakoutSettings.visualShakeGain;
        shakeEnergy += breakoutSettings.energyGain;

        // TRIGGER JUICE!
        JuiceEngine::Instance().TriggerGlitchKick(0.2f, 5.0f);

    }
    wasSpacePressed = isSpaceDown;

    // --------------------------------------------------------
    // ENERGY LOGIC 
    // --------------------------------------------------------
    // Apply Decay
    shakeEnergy -= breakoutSettings.energyDecay * elapsedTime;

    // Determine Checkpoints
    float minEnergy = 0.0f;
    if (gameStage >= 1) minEnergy = breakoutSettings.thresholdFormation;   
    if (gameStage >= 2) minEnergy = breakoutSettings.energyMax;            

    // Clamp Energy
    if (shakeEnergy < minEnergy) shakeEnergy = minEnergy;
    if (shakeEnergy > breakoutSettings.energyMax) shakeEnergy = breakoutSettings.energyMax;

    // --------------------------------------------------------
    // COLOR INTERPOLATION
    // --------------------------------------------------------
    const DirectX::XMFLOAT4& cStart = breakoutSettings.colorYellow; 
    const DirectX::XMFLOAT4& cPhase1 = breakoutSettings.colorPale;  
    const DirectX::XMFLOAT4& cPhase2 = breakoutSettings.colorWhite; 

    float t1 = breakoutSettings.thresholdFormation;
    float t2 = breakoutSettings.energyMax; 

    if (shakeEnergy <= t1)
    {
        // Phase 0 -> 1: Yellow to Pale
        float t = shakeEnergy / t1;
        if (t > 1.0f) t = 1.0f; if (t < 0.0f) t = 0.0f;

        color.x = cStart.x + (cPhase1.x - cStart.x) * t;
        color.y = cStart.y + (cPhase1.y - cStart.y) * t;
        color.z = cStart.z + (cPhase1.z - cStart.z) * t;
    }
    else
    {
        // Phase 1 -> 2: Pale to White
        float range = t2 - t1;
        float t = (shakeEnergy - t1) / range;
        if (t > 1.0f) t = 1.0f; if (t < 0.0f) t = 0.0f;

        color.x = cPhase1.x + (cPhase2.x - cPhase1.x) * t;
        color.y = cPhase1.y + (cPhase2.y - cPhase1.y) * t;
        color.z = cPhase1.z + (cPhase2.z - cPhase1.z) * t;
    }
    color.w = 1.0f;

    // --------------------------------------------------------
    // 4. VISUAL SHAKE PHYSICS
    // --------------------------------------------------------
    currentShakeIntensity -= breakoutSettings.visualShakeDecay * elapsedTime;

    if (currentShakeIntensity < 0.0f) currentShakeIntensity = 0.0f;
    if (currentShakeIntensity > breakoutSettings.visualMaxShake) currentShakeIntensity = breakoutSettings.visualMaxShake;

    if (currentShakeIntensity > 0.0f)
    {
        // Simple random shake offset
        float randX = ((float)(rand() % 100) / 50.0f - 1.0f) * currentShakeIntensity;
        float randZ = ((float)(rand() % 100) / 50.0f - 1.0f) * currentShakeIntensity;

        XMFLOAT3 shakePos = originalPosition;
        shakePos.x += randX;
        shakePos.z += randZ;
        movement->SetPosition(shakePos);
    }
    else
    {
        movement->SetPosition(originalPosition);
    }
}

void Player::UpdateEscapeLogic(float elapsedTime)
{
    float currentZ = movement->GetPosition().z;
    float diff = escapeTargetZ - currentZ;

    if (std::abs(diff) < 0.5f)
    {
        movement->SetMoveInput(0, 0); 
        return;
    }

    float dirZ = (diff > 0) ? 1.0f : -1.0f;
    movement->SetMoveInput(0, dirZ);
}

void Player::HandleMovementInput()
{
    float x = 0.0f;
    float z = 0.0f;

    // --- Keyboard Input ---
    if (GetAsyncKeyState('W') & 0x8000) z += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) z -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) x += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) x -= 1.0f;

    // --- Camera Relative Movement ---
    if (activeCamera)
    {
        XMFLOAT3 camFront = activeCamera->GetFront();
        XMFLOAT3 camRight = activeCamera->GetRight();

        // Flatten Y (biar tidak terbang/menukik saat nunduk/dongak)
        camFront.y = 0;
        camRight.y = 0;

        XMVECTOR vFront = XMLoadFloat3(&camFront);
        XMVECTOR vRight = XMLoadFloat3(&camRight);
        vFront = XMVector3Normalize(vFront);
        vRight = XMVector3Normalize(vRight);

        // Calculate Direction
        XMVECTOR vDir = (vRight * x) + (vFront * z);

        // Normalize vector (biar jalan miring tidak lebih cepat)
        if (XMVectorGetX(XMVector3LengthSq(vDir)) > 1.0f)
        {
            vDir = XMVector3Normalize(vDir);
        }

        XMFLOAT3 finalDir;
        XMStoreFloat3(&finalDir, vDir);

        movement->SetMoveInput(finalDir.x, finalDir.z);
    }
    else
    {
        // Fallback: World Space movement
        movement->SetMoveInput(x, z);
    }
}

bool Player::CheckJumpInput()
{
    return (GetAsyncKeyState(VK_SPACE) & 0x8000);
}