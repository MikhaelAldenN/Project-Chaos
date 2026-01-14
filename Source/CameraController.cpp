#include "CameraController.h"
#include "Camera.h"
#include "System/Input.h" 
#include <imgui.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// --- Helper Math Implementation ---
float CameraController::ApplyEasing(float t, EasingType type)
{
    // Clamp 0..1 just in case
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;

    switch (type)
    {
    case EasingType::EaseInQuad:    return t * t;
    case EasingType::EaseOutQuad:   return t * (2.0f - t);
    case EasingType::SmoothStep:    return t * t * (3.0f - 2.0f * t); // Unity/Unreal Standard
    case EasingType::Linear: default: return t;
    }
}

XMFLOAT3 CameraController::LerpFloat3(const XMFLOAT3& start, const XMFLOAT3& end, float t)
{
    XMVECTOR vStart = XMLoadFloat3(&start);
    XMVECTOR vEnd = XMLoadFloat3(&end);
    XMVECTOR vResult = XMVectorLerp(vStart, vEnd, t);

    XMFLOAT3 result;
    XMStoreFloat3(&result, vResult);
    return result;
}

XMFLOAT3 CameraController::LerpAngles(const XMFLOAT3& start, const XMFLOAT3& end, float t)
{
    // Sederhana: Linear Interpolation untuk Euler. 
    // Untuk rotasi kompleks (gimbal lock prevention), sebaiknya gunakan Quaternion Slerp,
    // tapi untuk cutscene sederhana ini sudah cukup efisien.
    return LerpFloat3(start, end, t);
}

CameraController::CameraController()
{
}

CameraController::~CameraController()
{
}

CameraController& CameraController::Instance()
{
    static CameraController instance;
    return instance;
}

void CameraController::SetActiveCamera(std::weak_ptr<Camera> camera)
{
    m_activeCamera = camera;
}

void CameraController::Update(float elapsedTime)
{
    std::shared_ptr<Camera> camera = m_activeCamera.lock();

    // SAFETY CHECK: If no camera is assigned, do nothing
    if (!camera) return;

    // --- Cursor Locking Logic (F1) ---
    static bool isF1Pressed = false;
    if (GetKeyState(VK_F1) & 0x8000)
    {
        if (!isF1Pressed)
        {
            if (controlMode == CameraControlMode::Mouse || controlMode == CameraControlMode::Free)
            {
                toggleCursor = !toggleCursor;
            }
            isF1Pressed = true;
        }
    }
    else
    {
        isF1Pressed = false;
    }

    if (controlMode == CameraControlMode::Sequence || controlMode == CameraControlMode::FixedStatic || controlMode == CameraControlMode::FixedFollow) {
        Input::Instance().GetMouse().LockCursor(false);
    }
    else {
        Input::Instance().GetMouse().LockCursor(toggleCursor);
    }

    // --- Main Control Logic ---
    switch (controlMode)
    {
    case CameraControlMode::Sequence:
    {
        // Safety: Jika queue kosong, kembali ke FixedStatic
        if (m_sequenceQueue.empty())
        {
            SetControlMode(CameraControlMode::FixedStatic);
            return;
        }

        // Ambil target saat ini
        const CameraKeyframe& targetKey = m_sequenceQueue[m_currentKeyframeIdx];

        // Update Timer
        m_seqTimer += elapsedTime;

        // Hitung progress (0.0 sampai 1.0)
        float t = 0.0f;
        if (targetKey.Duration > 0.0f) {
            t = m_seqTimer / targetKey.Duration;
        }
        else {
            t = 1.0f; // Instant move jika durasi 0
        }

        // Cek apakah keyframe selesai
        if (t >= 1.0f)
        {
            camera->SetPosition(targetKey.TargetPosition);
            camera->SetRotation(targetKey.TargetRotation);

            m_currentKeyframeIdx++;
            m_seqTimer = 0.0f;

            // Cek apakah sequence selesai
            if (m_currentKeyframeIdx >= m_sequenceQueue.size())
            {
                if (m_seqLoop)
                {
                    m_currentKeyframeIdx = 0;
                    m_seqStartPos = camera->GetPosition();
                    m_seqStartRot = camera->GetRotation();
                }
                else
                {
                    // === PERBAIKAN DI SINI ===
                    // Selesai: Stop di posisi terakhir
                    fixedPosition = camera->GetPosition();

                    // PENTING: Langsung ganti mode SEKARANG JUGA.
                    // Jangan tunggu frame berikutnya, karena GUI akan mencoba akses index yang kelebihan dan crash.
                    SetControlMode(CameraControlMode::FixedStatic);

                    return; // Keluar dari fungsi agar tidak lanjut ke bawah
                }
            }
            else
            {
                m_seqStartPos = camera->GetPosition();
                m_seqStartRot = camera->GetRotation();
            }
        }
        else
        {
            // Sedang bergerak: Hitung Interpolasi
            float easeT = ApplyEasing(t, targetKey.Easing);

            XMFLOAT3 newPos = LerpFloat3(m_seqStartPos, targetKey.TargetPosition, easeT);
            XMFLOAT3 newRot = LerpAngles(m_seqStartRot, targetKey.TargetRotation, easeT);

            camera->SetPosition(newPos);
            camera->SetRotation(newRot);

            // Sync variabel internal agar mode lain smooth jika di-switch
            this->eye = newPos;
            this->angle = newRot;
            this->fixedPosition = newPos;
        }
    }
    break;
    case CameraControlMode::FixedFollow:
    {
        // 1. Calculate desired position (Target + Offset)
        XMFLOAT3 finalPos;
        finalPos.x = target.x + fixedPosition.x;
        finalPos.y = target.y + fixedPosition.y;
        finalPos.z = target.z + fixedPosition.z;

        // 2. Apply to Camera
        camera->SetPosition(finalPos);
        camera->LookAt(target);
    }
    break;

    case CameraControlMode::FixedStatic:
    {
        // 1. Hitung Posisi Target + Offset
        XMVECTOR vTarget = XMLoadFloat3(&target);
        XMVECTOR vOffset = XMLoadFloat3(&m_targetOffset);
        XMVECTOR vFinalTarget = XMVectorAdd(vTarget, vOffset);

        XMFLOAT3 finalTargetPos;
        XMStoreFloat3(&finalTargetPos, vFinalTarget);

        camera->SetPosition(fixedPosition);

        // 2. LookAt standar (Ini akan mereset Roll jadi 0)
        camera->LookAt(finalTargetPos);

        // Re-Apply Roll dan Yaw Offset manual
        if (fixedYawOffset != 0.0f || fixedRollOffset != 0.0f)
        {
            XMFLOAT3 currentRot = camera->GetRotation();
            currentRot.y += fixedYawOffset;
            currentRot.z = fixedRollOffset;
            camera->SetRotation(currentRot);
        }
    }
    break;

    case CameraControlMode::Free:
    {
        // 1. Rotation (Mouse)
        if (toggleCursor)
        {
            Mouse& mouse = Input::Instance().GetMouse();
            float sensitivity = 0.5f;
            float rotSpeed = rollSpeed * elapsedTime;

            // Get current rotation from Camera to keep sync
            XMFLOAT3 currentRot = camera->GetRotation();

            // Update Pitch (X) and Yaw (Y)
            currentRot.y += static_cast<float>(mouse.GetDeltaX()) * sensitivity * rotSpeed; // Yaw
            currentRot.x += static_cast<float>(mouse.GetDeltaY()) * sensitivity * rotSpeed; // Pitch

            // Apply Rotation
            camera->SetRotation(currentRot);

            // Sync internal angle state
            this->angle = currentRot;
        }

        // 2. Movement (Keyboard)
        float moveAmount = moveSpeed * elapsedTime;
        XMFLOAT3 moveDir = { 0, 0, 0 };

        if (GetKeyState('W') & 0x8000) moveDir.z += moveAmount;
        if (GetKeyState('S') & 0x8000) moveDir.z -= moveAmount;
        if (GetKeyState('A') & 0x8000) moveDir.x -= moveAmount;
        if (GetKeyState('D') & 0x8000) moveDir.x += moveAmount;
        if (GetKeyState(VK_SPACE) & 0x8000)   moveDir.y += moveAmount;
        if (GetKeyState(VK_CONTROL) & 0x8000) moveDir.y -= moveAmount;

        camera->Translate(moveDir);
        this->eye = camera->GetPosition();
    }
    break;

    case CameraControlMode::GamePad:
    case CameraControlMode::Mouse:
    {
        // 1. Gather Input
        float ax = 0.0f;
        float ay = 0.0f;

        if (controlMode == CameraControlMode::GamePad)
        {
            GamePad& gamePad = Input::Instance().GetGamePad();
            ax = gamePad.GetAxisRX();
            ay = gamePad.GetAxisRY();
        }
        else if (controlMode == CameraControlMode::Mouse && toggleCursor)
        {
            Mouse& mouse = Input::Instance().GetMouse();
            float sensitivity = 0.5f;
            ax = static_cast<float>(mouse.GetDeltaX()) * sensitivity;
            ay = static_cast<float>(mouse.GetDeltaY()) * sensitivity;
        }

        // 2. Update Orbit Angles
        float speed = rollSpeed * elapsedTime;
        angle.x += ay * speed;
        angle.y += ax * speed;

        if (angle.x > maxAngleX) angle.x = maxAngleX;
        if (angle.x < minAngleX) angle.x = minAngleX;

        if (angle.y > XM_PI) angle.y -= XM_2PI;
        if (angle.y < -XM_PI) angle.y += XM_2PI;

        // 3. Calculate Orbit Position
        XMMATRIX matRot = XMMatrixRotationRollPitchYaw(angle.x, angle.y, 0.0f);
        XMVECTOR vOffset = XMVectorSet(0, 0, -range, 0);
        vOffset = XMVector3TransformCoord(vOffset, matRot);

        XMVECTOR vTarget = XMLoadFloat3(&target);
        XMVECTOR vEye = XMVectorAdd(vTarget, vOffset);

        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, vEye);

        camera->SetPosition(finalPos);
        camera->LookAt(target);

        this->eye = finalPos;
    }
    break;
    }
}

void CameraController::SetTarget(const DirectX::XMFLOAT3& target)
{
    this->target = target;
}

void CameraController::SetFixedSetting(const DirectX::XMFLOAT3& value)
{
    this->fixedPosition = value;
}

void CameraController::SetControlMode(CameraControlMode mode)
{
    if (this->controlMode != CameraControlMode::Mouse && mode == CameraControlMode::Mouse)
    {
        toggleCursor = true;
    }

    if (this->controlMode != CameraControlMode::Free && mode == CameraControlMode::Free)
    {
        if (auto camera = m_activeCamera.lock())
        {
            this->eye = camera->GetPosition();
            this->angle = camera->GetRotation();
        }
        toggleCursor = true;
    }

    this->controlMode = mode;
}

void CameraController::PlaySequence(const std::vector<CameraKeyframe>& sequence, bool loop)
{
    if (sequence.empty()) return;

    m_sequenceQueue = sequence;
    m_currentKeyframeIdx = 0;
    m_seqTimer = 0.0f;
    m_seqLoop = loop;

    // Ambil posisi awal kamera saat ini sebagai titik start
    auto camera = m_activeCamera.lock();
    if (camera)
    {
        m_seqStartPos = camera->GetPosition();
        m_seqStartRot = camera->GetRotation();
    }
    else
    {
        m_seqStartPos = { 0,0,0 };
        m_seqStartRot = { 0,0,0 };
    }

    SetControlMode(CameraControlMode::Sequence);
}

void CameraController::AppendKeyframe(const CameraKeyframe& keyframe)
{
    m_sequenceQueue.push_back(keyframe);
    // Jika kita sedang diam di akhir sequence, trigger logic untuk lanjut
    if (controlMode == CameraControlMode::Sequence && m_currentKeyframeIdx >= m_sequenceQueue.size() - 1)
    {
        // Logic penanganan dinamis bisa ditambahkan di sini
    }
}

void CameraController::StopSequence()
{
    m_sequenceQueue.clear();
    m_currentKeyframeIdx = 0;
    SetControlMode(CameraControlMode::FixedStatic);
}

void CameraController::DrawDebugGUI()
{
    auto camera = m_activeCamera.lock();
    if (!camera) return;

    ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
    ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Controller", nullptr, ImGuiWindowFlags_None))
    {
        // Mode Selection
        if (ImGui::RadioButton("Fixed Follow", controlMode == CameraControlMode::FixedFollow)) {
            SetControlMode(CameraControlMode::FixedFollow);
        }
        if (ImGui::RadioButton("Fixed Static", controlMode == CameraControlMode::FixedStatic)) {
            SetControlMode(CameraControlMode::FixedStatic);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("GamePad", controlMode == CameraControlMode::GamePad)) {
            SetControlMode(CameraControlMode::GamePad);
        }
        if (ImGui::RadioButton("Mouse", controlMode == CameraControlMode::Mouse)) {
            SetControlMode(CameraControlMode::Mouse);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Free (Debug)", controlMode == CameraControlMode::Free)) {
            SetControlMode(CameraControlMode::Free);
        }

        ImGui::Separator();

        // Info Display
        XMFLOAT3 camPos = camera->GetPosition();
        XMFLOAT3 camRot = camera->GetRotation();
        ImGui::Text("Pos: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
        ImGui::Text("Rot: %.2f, %.2f, %.2f", camRot.x, camRot.y, camRot.z);

        ImGui::Separator();

        // Mode Specific Controls
        if (controlMode == CameraControlMode::FixedFollow)
        {
            ImGui::Text("Fixed Settings");
            ImGui::DragFloat3("Offset", &fixedPosition.x, 0.1f);
        }
        else if (controlMode == CameraControlMode::FixedStatic)
        {
            ImGui::Text("Fixed Static Settings");
            ImGui::DragFloat3("Position", &fixedPosition.x, 0.1f);
        }
        else if (controlMode == CameraControlMode::Free)
        {
            ImGui::Text("Free Camera");
            ImGui::DragFloat("Speed", &moveSpeed, 0.1f, 1.0f, 100.0f);
        }
        else
        {
            ImGui::Text("Orbit Settings");
            ImGui::DragFloat("Range", &range, 0.1f, 1.0f, 100.0f);
        }
    }
    ImGui::End();

    if (ImGui::Begin("Sequence Controls"))
    {
        if (controlMode == CameraControlMode::Sequence)
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "SEQUENCER ACTIVE");
            if (!m_sequenceQueue.empty()) {
                ImGui::Text("Keyframe: %d / %d", m_currentKeyframeIdx + 1, (int)m_sequenceQueue.size());
                float progress = 0.0f;
                if (m_currentKeyframeIdx < m_sequenceQueue.size())
                    progress = m_seqTimer / m_sequenceQueue[m_currentKeyframeIdx].Duration;
                ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
            }
            else {
                ImGui::Text("Sequence Completed / Empty");
            }

            if (ImGui::Button("Stop Sequence")) {
                StopSequence();
            }
        }
    }
    ImGui::End();
}