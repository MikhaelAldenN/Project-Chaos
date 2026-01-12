#include "CameraController.h"
#include "Camera.h"
#include "System/Input.h" 
#include <imgui.h>
#include <cmath>

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
        XMVECTOR vTarget = XMLoadFloat3(&target);
        XMVECTOR vOffset = XMLoadFloat3(&m_targetOffset);
        XMVECTOR vFinalTarget = XMVectorAdd(vTarget, vOffset);

        XMFLOAT3 finalTargetPos;
        XMStoreFloat3(&finalTargetPos, vFinalTarget);

        activeCamera->SetPosition(fixedPosition);
        activeCamera->LookAt(finalTargetPos);
        if (fixedYawOffset != 0.0f)
        {
            XMFLOAT3 currentRot = activeCamera->GetRotation();
            currentRot.y += fixedYawOffset; 
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