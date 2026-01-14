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
}