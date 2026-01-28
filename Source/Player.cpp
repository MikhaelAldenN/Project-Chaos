#include "System/Input.h"
#include "System/Graphics.h" 
#include "AnimationController.h"
#include "Camera.h"
#include "Player.h"
#include "PlayerStates.h" 
#include "StateMachine.h"
#include "JuiceEngine.h"
#include <cmath>
#include <imgui.h> // [WAJIB]

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

        // [FIX] Masukkan elapsedTime ke sini
        HandleMovementInput(elapsedTime);
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

    if (m_isInvincible)
    {
        m_invincibleTimer -= elapsedTime;
        if (m_invincibleTimer <= 0.0f)
        {
            m_isInvincible = false;
            m_invincibleTimer = 0.0f;
        }
    }

    if (animator) animator->Update(elapsedTime);
    SyncData();
}

// [BARU] Implementasi SetPosition
void Player::SetPosition(float x, float y, float z)
{
    // 1. Update Posisi Fisika/Logic (CharacterMovement)
    if (movement)
    {
        movement->SetPosition({ x, y, z });
    }

    // 2. [PENTING] Update Anchor Position
    // Agar logika shake/breakout tidak mereset player ke 0,0,0
    originalPosition = { x, y, z };

    // 3. Update Visual Model Segera
    SyncData();
}

void Player::SetPosition(const DirectX::XMFLOAT3& pos)
{
    SetPosition(pos.x, pos.y, pos.z);
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
    isBreakoutActive = false;
}

void Player::UpdateBreakoutLogic(float elapsedTime)
{
    bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    // --------------------------------------------------------
    // INPUT HANDLING 
    // --------------------------------------------------------
    if (gameStage >= 2)
    {
        isSpaceDown = false;
    }

    if (isSpaceDown && !wasSpacePressed)
    {
        currentShakeIntensity += breakoutSettings.visualShakeGain;
        shakeEnergy += breakoutSettings.energyGain;

        // TRIGGER JUICE!
        JuiceEngine::Instance().TriggerGlitchKick(0.1f,1.7f);

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
        originalPosition = movement->GetPosition();
        isBreakoutActive = false;
        isEscaping = false;      
        isInputEnabled = true; 
        gameStage = 3;
        return;
    }

    movement->SetMoveInput(0.0f, 1.0f);
    if (diff > 0)
        movement->SetRotationY(0.0f);  
    else
        movement->SetRotationY(DirectX::XM_PI);
}

// File: Player.cpp

// Ubah definisi fungsi untuk menerima elapsedTime
void Player::HandleMovementInput(float elapsedTime)
{
    // 1. Ambil Raw Input (0, 1, atau -1)
    float targetX = 0.0f;
    float targetZ = 0.0f;

    if (GetAsyncKeyState('W') & 0x8000) targetZ += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) targetZ -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) targetX += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) targetX -= 1.0f;

    // Normalisasi Target agar diagonal tidak lebih cepat
    if (targetX != 0 || targetZ != 0)
    {
        float length = std::sqrt(targetX * targetX + targetZ * targetZ);
        targetX /= length;
        targetZ /= length;
    }

    // Handle Invert Controls (jika ada fitur ini)
    if (invertControls)
    {
        targetX = -targetX;
        targetZ = -targetZ;
    }

    // 2. APPLY SMOOTHING (ACCELERATION / DECELERATION)
    // Rumus: Current mendekati Target berdasarkan Acceleration * Time

    // Tentukan kecepatan transisi (Accel jika menekan tombol, Decel jika lepas)
    float blendX = (targetX != 0) ? acceleration : deceleration;
    float blendZ = (targetZ != 0) ? acceleration : deceleration;

    // Helper Lambda untuk Interpolasi Linear manual (MoveTowards)
    auto MoveTowards = [](float current, float target, float maxDelta) -> float {
        if (std::abs(target - current) <= maxDelta) return target;
        return current + std::copysign(maxDelta, target - current);
        };

    // Update nilai smooth input
    currentSmoothInput.x = MoveTowards(currentSmoothInput.x, targetX, blendX * elapsedTime);
    currentSmoothInput.y = MoveTowards(currentSmoothInput.y, targetZ, blendZ * elapsedTime);

    // 3. KIRIM KE MOVEMENT COMPONENT
    // Kita kalikan dengan moveSpeed agar karakter bergerak sesuai kecepatan yang diinginkan
    // Asumsi: SetMoveInput menerima vektor arah. 
    // Jika CharacterMovement kamu menghitung speed sendiri, kirim currentSmoothInput saja.
    // Jika CharacterMovement kamu butuh velocity, kalikan dengan moveSpeed.

    // OPSI A: Jika SetMoveInput mengharapkan Velocity (Arah * Speed)
    // movement->SetMoveInput(currentSmoothInput.x * moveSpeed, currentSmoothInput.y * moveSpeed);

    // OPSI B: Jika SetMoveInput mengharapkan Direction (0.0 - 1.0) dan speed diatur terpisah
    movement->SetMoveInput(
        currentSmoothInput.x * moveSpeed,
        currentSmoothInput.y * moveSpeed
    );
    // Rotasi Player (Instant Rotation ke arah input asli agar responsif)
    // Atau bisa pakai currentSmoothInput jika ingin rotasi ikut smooth
    /*
    if (targetX != 0 || targetZ != 0)
    {
         // Logika rotasi di sini
    }
    */
    // Untuk kode kamu yang sekarang, rotasi sepertinya di-lock atau diatur terpisah
    movement->SetRotationY(DirectX::XM_PI);
}

void Player::DrawDebugGUI()
{
    // Gunakan Collapsing Header agar rapi
    if (ImGui::CollapsingHeader("Player Movement Config", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Status: %s", (isInputEnabled ? "Input ON" : "Input OFF"));

        // 1. Slider Speed (Batas 0 sampai 50)
        ImGui::DragFloat("Max Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);

        // 2. Slider Acceleration (Responsivitas saat tekan tombol)
        // Semakin besar = semakin instan geraknya (seperti Mario vs Sonic)
        ImGui::DragFloat("Acceleration", &acceleration, 0.1f, 0.1f, 100.0f);

        // 3. Slider Deceleration (Licin tidaknya lantai)
        // Semakin kecil = semakin licin (seperti di es)
        // Semakin besar = berhenti instan
        ImGui::DragFloat("Deceleration", &deceleration, 0.1f, 0.1f, 100.0f);

        ImGui::Separator();

        // Monitor Nilai Input Realtime
        ImGui::Text("Raw Input Smooth: (%.2f, %.2f)", currentSmoothInput.x, currentSmoothInput.y);
    }
}