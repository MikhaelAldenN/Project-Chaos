#include "System/Input.h"
#include "System/Graphics.h" 
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerStates.h" 
#include "StateMachine.h"
#include <cmath>
#include <imgui.h>

using namespace DirectX;

Player::Player()
{
    // Init Model
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");
    scale = defaultScale;

    // Init Animation Controller
    animator = new AnimationController();
    animator->Initialize(model);

    // Init State Machine
    stateMachine = new StateMachine();
    stateMachine->Initialize(new PlayerIdle(), this);

    // Init Gravity
    movement->SetGravityEnabled(false);

    // Default Color
    color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

Player::~Player()
{
    if (stateMachine) delete stateMachine;
    if (animator) delete animator;
}

void Player::Update(float elapsedTime, Camera* camera)
{
    SetCamera(camera);
    if (isInputEnabled) HandleMovementInput(elapsedTime);
    else currentSmoothInput = { 0.0f, 0.0f };

    UpdateHorizontalMovement(elapsedTime);

    if (stateMachine) stateMachine->Update(this, elapsedTime);
    if (animator) animator->Update(elapsedTime);
    if (movement) movement->Update(elapsedTime);

    // Apply scaling and transform
    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rot.x),
        XMConvertToRadians(rot.y),
        XMConvertToRadians(rot.z)
    );
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    if (model) model->UpdateTransform(worldMatrix);
}

void Player::HandleMovementInput(float dt)
{
    float targetX = 0.0f;
    float targetZ = 0.0f;

    // Use GetAsyncKeyState directly. 
    // & 0x8000 checks if the key is currently being held down.
    if (GetAsyncKeyState('W') & 0x8000) targetZ = 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) targetZ = -1.0f;
    if (GetAsyncKeyState('A') & 0x8000) targetX = -1.0f;
    if (GetAsyncKeyState('D') & 0x8000) targetX = 1.0f;

    if (invertControls)
    {
        targetX = -targetX;
        targetZ = -targetZ;
    }

    if (targetX != 0.0f && targetZ != 0.0f)
    {
        float length = std::sqrt(targetX * targetX + targetZ * targetZ);
        targetX /= length;
        targetZ /= length;
    }

    // X Axis Smoothing
    if (targetX != 0.0f)
        currentSmoothInput.x += (targetX - currentSmoothInput.x) * acceleration * dt;
    else
        currentSmoothInput.x += (0.0f - currentSmoothInput.x) * deceleration * dt;

    // Z Axis Smoothing (Stored in Y of currentSmoothInput)
    if (targetZ != 0.0f)
        currentSmoothInput.y += (targetZ - currentSmoothInput.y) * acceleration * dt;
    else
        currentSmoothInput.y += (0.0f - currentSmoothInput.y) * deceleration * dt;

    // Cutoff to prevent micro-drifting
    if (std::abs(currentSmoothInput.x) < 0.01f) currentSmoothInput.x = 0.0f;
    if (std::abs(currentSmoothInput.y) < 0.01f) currentSmoothInput.y = 0.0f;
}

void Player::UpdateHorizontalMovement(float elapsedTime)
{
    movement->SetVelocity(DirectX::XMFLOAT3(
        currentSmoothInput.x * moveSpeed,
        0.0f,
        currentSmoothInput.y * moveSpeed
    ));

    movement->SetRotationY(DirectX::XM_PI);
}

void Player::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Player Movement Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Status: %s", (isInputEnabled ? "Input ON" : "Input OFF"));

        ImGui::DragFloat("Max Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Acceleration", &acceleration, 0.1f, 0.1f, 100.0f);
        ImGui::DragFloat("Deceleration", &deceleration, 0.1f, 0.1f, 100.0f);

        ImGui::Checkbox("Invert Controls", &invertControls);
    }
}

void Player::SetPosition(float x, float y, float z)
{
    if (movement) movement->SetPosition({ x, y, z });
}

void Player::SetPosition(const DirectX::XMFLOAT3& pos)
{
    if (movement) movement->SetPosition(pos);
}