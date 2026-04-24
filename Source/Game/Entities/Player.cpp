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
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_player.glb");
    scale = defaultScale;

    animator = new AnimationController();
    animator->Initialize(model);

    stateMachine = new StateMachine();
    stateMachine->Initialize(new PlayerIdle(), this);

    movement->SetGravityEnabled(false);
    color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

Player::~Player()
{
    if (stateMachine) delete stateMachine;
    if (animator) delete animator;
    if (m_physxController) m_physxController->release(); 
}

// [PHYSX ADDED] Initialize the capsule
void Player::InitPhysics(physx::PxControllerManager* manager, physx::PxMaterial* material)
{
    physx::PxCapsuleControllerDesc desc;
    desc.height = 2.0f;
    desc.radius = 0.5f;
    desc.position = physx::PxExtendedVec3(0.0, 15.0, 0.0); // Start in the air
    desc.material = material;
    desc.stepOffset = 0.5f;

    m_physxController = manager->createController(desc);
}

void Player::Update(float elapsedTime, Camera* camera)
{
    if (!canDash) {
        dashCooldownTimer -= elapsedTime;
        if (dashCooldownTimer <= 0.0f) {
            canDash = true;
        }
    }

    SetCamera(camera);
    if (isInputEnabled) HandleMovementInput(elapsedTime);
    else currentSmoothInput = { 0.0f, 0.0f };

    UpdateHorizontalMovement(elapsedTime);

    if (stateMachine) stateMachine->Update(this, elapsedTime);
    if (animator) animator->Update(elapsedTime);
    if (movement) movement->Update(elapsedTime);

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rot.x), XMConvertToRadians(rot.y), XMConvertToRadians(rot.z));
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    if (model) model->UpdateTransform(worldMatrix);
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

    if (targetX != 0.0f && targetZ != 0.0f) {
        float length = std::sqrt(targetX * targetX + targetZ * targetZ);
        targetX /= length; targetZ /= length;
    }

    if (targetX != 0.0f) currentSmoothInput.x += (targetX - currentSmoothInput.x) * acceleration * dt;
    else currentSmoothInput.x += (0.0f - currentSmoothInput.x) * deceleration * dt;

    if (targetX != 0.0f || targetZ != 0.0f) {
        lastValidInput = { targetX, targetZ };
    }

    if (targetZ != 0.0f) currentSmoothInput.y += (targetZ - currentSmoothInput.y) * acceleration * dt;
    else currentSmoothInput.y += (0.0f - currentSmoothInput.y) * deceleration * dt;

    if (std::abs(currentSmoothInput.x) < 0.01f) currentSmoothInput.x = 0.0f;
    if (std::abs(currentSmoothInput.y) < 0.01f) currentSmoothInput.y = 0.0f;
}

void Player::UpdateHorizontalMovement(float elapsedTime)
{
    // 1. Kalkulasi vektor perpindahan dasar
    float displacementX = currentSmoothInput.x * moveSpeed * elapsedTime;
    float displacementZ = currentSmoothInput.y * moveSpeed * elapsedTime;

    // 2. Cabang Logika: Pakai PhysX (punya temanmu) ATAU Kinematic murni (punyamu)
    if (m_physxController)
    {
        // --- MODE PHYSX (SceneGameBreaker) ---
        float gravityY = -9.81f * elapsedTime;
        physx::PxVec3 displacement(displacementX, gravityY, displacementZ);

        m_physxController->move(displacement, 0.001f, elapsedTime, physx::PxControllerFilters());

        physx::PxExtendedVec3 pxPos = m_physxController->getPosition();
        movement->SetPosition({ (float)pxPos.x, (float)pxPos.y - 1.5f, (float)pxPos.z });
    }
    else
    {
        // --- MODE KINEMATIC (SceneBoss) ---
        // Jika PhysX tidak diinisialisasi, gerakkan koordinat secara manual
        DirectX::XMFLOAT3 currentPos = movement->GetPosition();
        currentPos.x += displacementX;
        currentPos.z += displacementZ;

        // (Asumsi di SceneBoss gravitasi diurus terpisah atau tidak ada)
        movement->SetPosition(currentPos);
    }
}

void Player::RotateModelToPoint(const DirectX::XMFLOAT3& targetPos)
{
    DirectX::XMFLOAT3 currentPos = GetPosition();
    float dx = targetPos.x - currentPos.x;
    float dz = targetPos.z - currentPos.z;
    float angleRadians = atan2f(dx, dz);
    float angleDegrees = DirectX::XMConvertToDegrees(angleRadians);
    movement->SetRotationY(angleDegrees);
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