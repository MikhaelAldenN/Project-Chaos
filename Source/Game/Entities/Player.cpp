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

    for (auto& bullet : m_projectiles) {
        if (bullet->IsActive()) bullet->Update(elapsedTime, camera);
    }
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
    float displacementX = 0.0f;
    float displacementZ = 0.0f;

    // Grab the velocity set by the State Machine (e.g., Dash)
    DirectX::XMFLOAT3 stateVelocity = movement->GetVelocity();

    // If the state machine is forcing a velocity (like dashing), use it
    if (stateVelocity.x != 0.0f || stateVelocity.z != 0.0f)
    {
        displacementX = stateVelocity.x * elapsedTime;
        displacementZ = stateVelocity.z * elapsedTime;
    }
    // Otherwise, calculate normal walking movement
    else
    {
        displacementX = currentSmoothInput.x * moveSpeed * elapsedTime;
        displacementZ = currentSmoothInput.y * moveSpeed * elapsedTime;
    }

    // 2. Cabang Logika: Pakai PhysX ATAU Kinematic murni
    if (m_physxController)
    {
        // --- MODE PHYSX (SceneGameBreaker / SceneGame) ---
        float gravityY = -9.81f * elapsedTime;
        physx::PxVec3 displacement(displacementX, gravityY, displacementZ);

        m_physxController->move(displacement, 0.001f, elapsedTime, physx::PxControllerFilters());

        physx::PxExtendedVec3 pxPos = m_physxController->getPosition();
        movement->SetPosition({ (float)pxPos.x, (float)pxPos.y - 1.5f, (float)pxPos.z });
    }
    else
    {
        // --- MODE KINEMATIC (SceneBoss) ---
        DirectX::XMFLOAT3 currentPos = movement->GetPosition();
        currentPos.x += displacementX;
        currentPos.z += displacementZ;

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

void Player::FireProjectile()
{
    auto newBullet = std::make_unique<Bullet>();
    DirectX::XMFLOAT3 myPos = movement->GetPosition();

    float yawRad = DirectX::XMConvertToRadians(movement->GetRotation().y);
    DirectX::XMFLOAT3 fwd = { sinf(yawRad), 0.0f, cosf(yawRad) };
    
    // Spawn slightly in front of the player
    myPos.x += fwd.x * 1.5f;
    myPos.z += fwd.z * 1.5f;
    myPos.y += 1.0f;

    float bulletSpeed = 25.0f;
    newBullet->Fire(myPos, fwd, bulletSpeed);

    m_projectiles.push_back(std::move(newBullet));

    // Limit bullet count to prevent memory leaks
    if (m_projectiles.size() > 5) {
        m_projectiles.pop_front();
    }
}

void Player::RenderProjectiles(ModelRenderer* renderer)
{
    for (auto& bullet : m_projectiles)
    {
        if (bullet->IsActive())
        {
            renderer->Draw(ShaderId::Phong, bullet->GetModel(), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
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