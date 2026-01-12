#include "System/Input.h"
#include "System/Graphics.h" 
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerStates.h" 
#include "StateMachine.h"
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
}

Player::~Player()
{
    if (stateMachine) delete stateMachine;
    if (animator) delete animator;
}

void Player::Update(float elapsedTime, Camera* camera)
{
    // Simpan referensi camera frame ini
    SetCamera(camera);

    // 2. Physics 
    if (isInputEnabled)
    {
        if (stateMachine) stateMachine->Update(this, elapsedTime);
        movement->Update(elapsedTime);
    }

    // Breakout Logic
    if (isBreakoutActive)
    {
        UpdateBreakoutLogic(elapsedTime);
    }

    // 3. Visuals (Animator hitung pose)
    if (animator) animator->Update(elapsedTime);

    // 4. Sync (Physics -> Graphics)
    SyncData();
}

void Player::SetBreakoutMode(bool enable)
{
    isBreakoutActive = enable;

    if (enable)
    {
        originalPosition = movement->GetPosition();
        currentShakeIntensity = 0.0f; 
    }
    else
    {
        movement->SetPosition(originalPosition);
    }
}

void Player::UpdateBreakoutLogic(float elapsedTime)
{
    bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    if (isSpaceDown && !wasSpacePressed)
    {
        currentShakeIntensity += breakoutSettings.shakeGain;
    }

    wasSpacePressed = isSpaceDown;

    currentShakeIntensity -= breakoutSettings.shakeDecay * elapsedTime;

    if (currentShakeIntensity < 0.0f) currentShakeIntensity = 0.0f;
    if (currentShakeIntensity > breakoutSettings.maxShake) currentShakeIntensity = breakoutSettings.maxShake;
    if (currentShakeIntensity > 0.0f)
    {
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

    // PENTING: JANGAN panggil movement->Jump() di sini!
    // Itu tugas State PlayerJump::Enter().
}

bool Player::CheckJumpInput()
{
    return (GetAsyncKeyState(VK_SPACE) & 0x8000);
}