#include "System/Input.h"
#include "System/Graphics.h" 
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerStates.h" 
#include "StateMachine.h"
#include <cmath>
#include <imgui.h>
#include "InputHelper.h"

using namespace DirectX;

Player::Player()
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/TEST_mdl_Player3.glb");
    //scale = defaultScale;
    scale = {1.0f, 1.0f, 1.0f};

    animator = new AnimationController();
    animator->Initialize(model);
    animator->SetUpperBodyMaskRoot("body");
    stateMachine = new StateMachine();
    stateMachine->Initialize(new PlayerIdle(), this);

    OutputDebugStringA("\n=== ANIMATIONS LOADED ===\n");
    const auto& anims = model->GetAnimations();
    for (size_t i = 0; i < anims.size(); ++i) {
        std::string msg = "[" + std::to_string(i) + "] " + anims[i].name + "\n";
        OutputDebugStringA(msg.c_str());
    }
    OutputDebugStringA("=========================\n\n");

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

    // ==========================================
    // TRIGGER PARRY (UPPER BODY ONLY)
    // ==========================================
    if (isInputEnabled && Input::Instance().GetKeyboard().IsTriggered(VK_SPACE))
    {
        // Cegah spam spasi dengan mengecek apakah animasi masih berjalan
        if (animator && !animator->IsUpperPlaying())
        {
            // Panggil nama animasimu (pastikan namanya sama persis dengan yang di Blender)
            animator->PlayUpper("Parry", false);
        }
    }

    SetCamera(camera);
    if (isInputEnabled) HandleMovementInput(elapsedTime);
    else currentSmoothInput = { 0.0f, 0.0f };

    UpdateHorizontalMovement(elapsedTime);

    if (stateMachine) stateMachine->Update(this, elapsedTime);
    if (animator) animator->Update(elapsedTime);
    if (movement) movement->Update(elapsedTime);

    // ==========================================
        // 1. KALKULASI ARAH KAKI (DENGAN SMOOTHING)
        // ==========================================
    float rotationSmoothSpeed = 15.0f; // Bisa kamu pindah ke Player.h nanti untuk tuning
    DirectX::XMFLOAT2 moveInput = GetLastValidInput();

    // Ambil rotasi kaki SAAT INI dari memori (dalam Radian)
    float currentBaseYaw = XMConvertToRadians(movement->GetRotation().y);
    float targetBaseYaw = currentBaseYaw;

    // Jika sedang menekan tombol jalan, ubah target arah
    if (moveInput.x != 0.0f || moveInput.y != 0.0f) {
        targetBaseYaw = atan2(moveInput.x, moveInput.y);
    }

    // Hitung jarak sudut yang harus ditempuh
    float angleDiff = targetBaseYaw - currentBaseYaw;

    // Normalisasi selisih sudut agar mencari jalur terpendek (-PI sampai PI)
    // (Mencegah karakter muter balik 300 derajat untuk belok sedikit)
    while (angleDiff > XM_PI)  angleDiff -= XM_2PI;
    while (angleDiff < -XM_PI) angleDiff += XM_2PI;

    // Terapkan Smoothing (Lerp)
    float lerpFactor = rotationSmoothSpeed * elapsedTime;
    if (lerpFactor > 1.0f) lerpFactor = 1.0f; // Mencegah overshoot

    // INI ADALAH ROTASI BARU UNTUK FRAME INI
    float smoothedYaw = currentBaseYaw + (angleDiff * lerpFactor);

    // ==========================================
    // 2. LIMITASI AIM & TARIK KAKI (AIM PULLING)
    // ==========================================
    bool shouldAim = false;
    float finalRelativeAngle = 0.0f;

    if (model && activeCamera)
    {
        DirectX::XMFLOAT3 pos = movement->GetPosition();
        DirectX::XMFLOAT3 mousePos = Beyond::InputHelper::GetMouseWorldPos(activeCamera->GetPosition());

        float dx = mousePos.x - pos.x;
        float dz = mousePos.z - pos.z;
        float distanceSq = (dx * dx) + (dz * dz);

        // Deadzone: Hanya jalankan jika kursor tidak terlalu dekat dengan karakter
        if (distanceSq > 0.1f)
        {
            shouldAim = true;

            float absoluteAngleToMouse = atan2(dx, dz);

            // [KUNCI NYA DI SINI]: Kita membandingkan kursor dengan kaki yang SUDAH HALUS
            float relativeAngle = absoluteAngleToMouse - smoothedYaw;

            // Normalisasi sudut torso
            while (relativeAngle > XM_PI) relativeAngle -= XM_2PI;
            while (relativeAngle < -XM_PI) relativeAngle += XM_2PI;

            // Batas maksimal putaran tulang belakang (90 derajat Kiri/Kanan)
            float maxTorsoAngle = XM_PIDIV2;

            if (relativeAngle > maxTorsoAngle)
            {
                relativeAngle = maxTorsoAngle;
                // Aim Pulling: Jika torso mentok, paksa tarik kakinya
                smoothedYaw = absoluteAngleToMouse - relativeAngle;
            }
            else if (relativeAngle < -maxTorsoAngle)
            {
                relativeAngle = -maxTorsoAngle;
                // Aim Pulling: Jika torso mentok, paksa tarik kakinya
                smoothedYaw = absoluteAngleToMouse - relativeAngle;
            }

            finalRelativeAngle = relativeAngle;
        }
    }

    // ==========================================
        // 3. TERAPKAN ROTASI KE KAKI (BASE) DAN MATRIX
        // ==========================================
    movement->SetRotationY(DirectX::XMConvertToDegrees(smoothedYaw));

    XMFLOAT3 pos = movement->GetPosition();
    XMFLOAT3 rot = movement->GetRotation();

    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);

    // Gunakan rot.y murni tanpa hack
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rot.x), XMConvertToRadians(rot.y), XMConvertToRadians(rot.z));
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 worldMatrix;
    XMStoreFloat4x4(&worldMatrix, S * R * T);

    // ==========================================
    // 4. INJEKSI ROTASI BADAN ATAS (TORSO)
    // ==========================================
    if (shouldAim && model)
    {
        int bodyIndex = model->GetNodeIndex("body");
        if (bodyIndex != -1)
        {
            Model::Node& bodyNode = model->GetNodes()[bodyIndex];

            // KITA HAPUS HACK XM_PI DI SINI! Cukup gunakan finalRelativeAngle murni.
            // Karena kakinya sudah kita putar ke arah yang benar di step 3, torsonya otomatis sembuh!
            XMVECTOR aimRot = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), finalRelativeAngle);
            XMVECTOR currentLocalRot = XMLoadFloat4(&bodyNode.rotation);
            XMVECTOR finalRot = XMQuaternionMultiply(currentLocalRot, aimRot);
            XMStoreFloat4(&bodyNode.rotation, finalRot);
        }
    }

    // Update hirarki Model
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

        if (elapsedTime > 0.0f) {
            movement->SetVelocity(DirectX::XMFLOAT3(
                displacementX / elapsedTime, 
                0.0f, 
                displacementZ / elapsedTime
            ));
        }
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