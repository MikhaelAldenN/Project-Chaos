#include "CameraController.h"
#include "Camera.h"
#include "System/Input.h" 
#include <imgui.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

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

void CameraController::SetActiveCamera(Camera* camera)
{
    activeCamera = camera;
}

XMVECTOR EulerToQuat(const XMFLOAT3& rot)
{
    return XMQuaternionRotationRollPitchYaw(rot.x, rot.y, rot.z);
}

// Helper Easing
float CameraController::ApplyEasing(float t, EasingType type)
{
    switch (type)
    {
    case EasingType::EaseInQuad: return t * t;
    case EasingType::EaseOutQuad: return t * (2 - t);
    case EasingType::SmoothStep: return t * t * (3 - 2 * t);
    case EasingType::Linear: default: return t;
    }
}

void CameraController::StartTransition(Camera* targetCamSettings, float duration, EasingType easing)
{
    if (!activeCamera || !targetCamSettings) return;

    // 1. Buat daftar jalur (path) sementara
    std::vector<Camera*> path;
    path.push_back(activeCamera);      // Titik Awal (Kamera saat ini)
    path.push_back(targetCamSettings); // Titik Akhir (Target)

    // 2. Oper ke fungsi utama 'StartPathTransition'
    StartPathTransition(path, duration, easing);
}

void CameraController::StartPathTransition(const std::vector<Camera*>& keyframes, float duration, EasingType easing)
{
    if (!activeCamera || keyframes.size() < 2) return;

    if (controlMode != CameraControlMode::Transition)
    {
        previousMode = controlMode;
    }

    controlMode = CameraControlMode::Transition;

    // Reset State
    transition.active = true;
    transition.currentTime = 0.0f;
    transition.duration = duration > 0.0f ? duration : 0.001f;
    transition.easingType = easing;
    transition.keyframes.clear();

    // 1. Snapshot Data dari semua kamera input
    for (Camera* cam : keyframes)
    {
        TransitionState::KeyframeData data;
        data.pos = cam->GetPosition();
        XMStoreFloat4(&data.rotQuat, EulerToQuat(cam->GetRotation()));
        data.roll = cam->GetRotation().z;
        transition.keyframes.push_back(data);
    }

    // Simpan target lookat dari kamera terakhir untuk safety saat finish
    if (!keyframes.empty())
    {
        transition.finalTargetLookAt = keyframes.back()->GetFocus();
        fixedRollOffset = keyframes.back()->GetRotation().z; // Set roll offset target
        fixedYawOffset = 0.0f;
    }
}

void CameraController::Update(float elapsedTime)
{
    // SAFETY CHECK: If no camera is assigned, do nothing
    if (activeCamera == nullptr) return;

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

    if (controlMode == CameraControlMode::Mouse || controlMode == CameraControlMode::Free) {
        Input::Instance().GetMouse().LockCursor(toggleCursor);
    }
    else {
        Input::Instance().GetMouse().LockCursor(false);
    }

    // --- Main Control Logic ---
    switch (controlMode)
    {
    case CameraControlMode::Transition:
    {
        if (!transition.active || transition.keyframes.empty()) return;

        // 1. Update Global Time (0.0 sampai 1.0)
        transition.currentTime += elapsedTime;
        float t = std::clamp(transition.currentTime / transition.duration, 0.0f, 1.0f);

        // [GLOBAL EASING] Ini kuncinya! Easing diterapkan pada total durasi.
        // Jadi B tidak akan kena "slow in/slow out", dia akan dilewati dengan cepat.
        float globalT = ApplyEasing(t, transition.easingType);

        // 2. Tentukan Segmen Mana Kita Berada
        // Contoh: A->B->C (3 titik = 2 segmen). Segmen 1 (0-0.5), Segmen 2 (0.5-1.0)
        size_t numSegments = transition.keyframes.size() - 1;
        float segmentDuration = 1.0f / numSegments;

        size_t currentSegment = static_cast<size_t>(globalT / segmentDuration);
        if (currentSegment >= numSegments) currentSegment = numSegments - 1;

        // 3. Hitung Local T (0.0 - 1.0) di dalam segmen tersebut
        float segmentStartT = currentSegment * segmentDuration;
        float localT = (globalT - segmentStartT) / segmentDuration;

        // 4. Interpolasi Antara Keyframe[i] dan Keyframe[i+1]
        const auto& startFrame = transition.keyframes[currentSegment];
        const auto& endFrame = transition.keyframes[currentSegment + 1];

        // --- Lerp Position ---
        XMVECTOR vStartPos = XMLoadFloat3(&startFrame.pos);
        XMVECTOR vEndPos = XMLoadFloat3(&endFrame.pos);
        XMVECTOR vCurrentPos = XMVectorLerp(vStartPos, vEndPos, localT);

        // --- Slerp Rotation ---
        XMVECTOR qStart = XMLoadFloat4(&startFrame.rotQuat);
        XMVECTOR qEnd = XMLoadFloat4(&endFrame.rotQuat);
        XMVECTOR qCurrent = XMQuaternionSlerp(qStart, qEnd, localT);

        // --- Lerp Roll (Manual agar presisi) ---
        float currentRoll = startFrame.roll + (endFrame.roll - startFrame.roll) * localT;

        // 5. Apply to Active Camera
        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, vCurrentPos);
        activeCamera->SetPosition(finalPos);

        XMFLOAT4X4 matRot;
        XMStoreFloat4x4(&matRot, XMMatrixRotationQuaternion(qCurrent));

        // Math Safety Clamp
        float sinPitch = std::clamp(-matRot._32, -1.0f, 1.0f);
        float pitch = asinf(sinPitch);
        float yaw = atan2f(matRot._31, matRot._33);

        activeCamera->SetRotation(pitch, yaw, currentRoll);

        // 6. Finish Logic
        if (t >= 1.0f)
        {
            controlMode = CameraControlMode::FixedStatic;
            fixedPosition = finalPos;
            target = transition.finalTargetLookAt;

            // Simpan Roll akhir ke offset agar tidak reset saat masuk FixedStatic
            fixedRollOffset = endFrame.roll;
            fixedYawOffset = 0.0f;

            transition.active = false;
        }
        break;
    }
    case CameraControlMode::FixedFollow:
    {
        // 1. Calculate desired position (Target + Offset)
        XMFLOAT3 finalPos;
        finalPos.x = target.x + fixedPosition.x;
        finalPos.y = target.y + fixedPosition.y;
        finalPos.z = target.z + fixedPosition.z;

        // 2. Apply to Camera
        activeCamera->SetPosition(finalPos);
        activeCamera->LookAt(target);
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

        activeCamera->SetPosition(fixedPosition);

        // 2. LookAt standar (Ini akan mereset Roll jadi 0)
        activeCamera->LookAt(finalTargetPos);

        // [BARU & PENTING] Re-Apply Roll dan Yaw Offset
        if (fixedYawOffset != 0.0f || fixedRollOffset != 0.0f)
        {
            XMFLOAT3 currentRot = activeCamera->GetRotation();

            // Tambahkan Yaw (jika ada animasi putar)
            currentRot.y += fixedYawOffset;

            // Force Roll sesuai settingan (misal 90 derajat dari Camera B)
            currentRot.z = fixedRollOffset;

            activeCamera->SetRotation(currentRot);
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
            XMFLOAT3 currentRot = activeCamera->GetRotation();

            // Update Pitch (X) and Yaw (Y)
            currentRot.y += static_cast<float>(mouse.GetDeltaX()) * sensitivity * rotSpeed; // Yaw
            currentRot.x += static_cast<float>(mouse.GetDeltaY()) * sensitivity * rotSpeed; // Pitch

            // Apply Rotation
            activeCamera->SetRotation(currentRot);

            // Sync internal angle state
            this->angle = currentRot;
        }

        // 2. Movement (Keyboard - Local Space / Unity "Translate")
        float moveAmount = moveSpeed * elapsedTime;
        XMFLOAT3 moveDir = { 0, 0, 0 };

        if (GetKeyState('W') & 0x8000) moveDir.z += moveAmount; // Forward
        if (GetKeyState('S') & 0x8000) moveDir.z -= moveAmount; // Backward
        if (GetKeyState('A') & 0x8000) moveDir.x -= moveAmount; // Left
        if (GetKeyState('D') & 0x8000) moveDir.x += moveAmount; // Right
        if (GetKeyState(VK_SPACE) & 0x8000)   moveDir.y += moveAmount; // Up (Global or Local depending on implementation)
        if (GetKeyState(VK_CONTROL) & 0x8000) moveDir.y -= moveAmount; // Down

        // Apply Movement
        activeCamera->Translate(moveDir);

        // Update internal 'eye' state for debug display
        this->eye = activeCamera->GetPosition();
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
        angle.x += ay * speed; // Pitch
        angle.y += ax * speed; // Yaw

        // Clamp Pitch
        if (angle.x > maxAngleX) angle.x = maxAngleX;
        if (angle.x < minAngleX) angle.x = minAngleX;

        // Wrap Yaw
        if (angle.y > XM_PI) angle.y -= XM_2PI;
        if (angle.y < -XM_PI) angle.y += XM_2PI;

        // 3. Calculate Orbit Position using Spherical Coordinates logic
        // Create rotation matrix from angles
        XMMATRIX matRot = XMMatrixRotationRollPitchYaw(angle.x, angle.y, 0.0f);

        // Define offset (behind target)
        XMVECTOR vOffset = XMVectorSet(0, 0, -range, 0); // Start behind

        // Rotate the offset
        vOffset = XMVector3TransformCoord(vOffset, matRot);

        // Add to target position
        XMVECTOR vTarget = XMLoadFloat3(&target);
        XMVECTOR vEye = XMVectorAdd(vTarget, vOffset);

        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, vEye);

        // 4. Apply to Camera
        activeCamera->SetPosition(finalPos);
        activeCamera->LookAt(target);

        // Update internal 'eye' state
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
    // Logika sinkronisasi saat ganti mode

    // Jika masuk mode Mouse, pastikan kursor aktif/tidak sesuai setting
    if (this->controlMode != CameraControlMode::Mouse && mode == CameraControlMode::Mouse)
    {
        toggleCursor = true;
    }

    // Jika masuk mode Free, ambil posisi kamera saat ini agar transisi mulus
    if (this->controlMode != CameraControlMode::Free && mode == CameraControlMode::Free)
    {
        if (activeCamera)
        {
            // Sync posisi internal Free Cam dengan posisi kamera asli
            this->eye = activeCamera->GetPosition();

            // Sync sudut pandang juga agar tidak 'kaget'
            this->angle = activeCamera->GetRotation();
        }
        toggleCursor = true;
    }

    this->controlMode = mode;
}

void CameraController::DrawDebugGUI()
{
    if (!activeCamera) return;

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
        XMFLOAT3 camPos = activeCamera->GetPosition();
        XMFLOAT3 camRot = activeCamera->GetRotation(); // Euler
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
        else // Orbit Modes
        {
            ImGui::Text("Orbit Settings");
            ImGui::DragFloat("Range", &range, 0.1f, 1.0f, 100.0f);
        }
    }
    ImGui::End();
}