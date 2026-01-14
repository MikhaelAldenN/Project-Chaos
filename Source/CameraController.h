#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

class Camera;

enum class CameraControlMode
{
    GamePad,
    Mouse,
    FixedFollow,
    FixedStatic,
    Free
    // Removed: Transition
};

class CameraController
{
public:
    static CameraController& Instance();

    void SetActiveCamera(std::weak_ptr<Camera> camera);
    std::shared_ptr<Camera> GetActiveCamera() const { return m_activeCamera.lock(); }

    void Update(float elapsedTime);
    void DrawDebugGUI();

    // --- Basic Controls ---
    void SetTarget(const DirectX::XMFLOAT3& target);
    void SetTargetOffset(const DirectX::XMFLOAT3& offset) { m_targetOffset = offset; }
    void SetFixedSetting(const DirectX::XMFLOAT3& value);

    // Offset manual untuk koreksi rotasi (jika diperlukan)
    void SetFixedYawOffset(float radians) { fixedYawOffset = radians; }
    void SetFixedRollOffset(float radians) { fixedRollOffset = radians; }

    void SetControlMode(CameraControlMode mode);
    CameraControlMode GetControlMode() const { return controlMode; }
    void ClearCamera() { m_activeCamera.reset(); }

private:
    CameraController();
    ~CameraController();
    CameraController(const CameraController&) = delete;
    void operator=(const CameraController&) = delete;

    std::weak_ptr<Camera> m_activeCamera;
    CameraControlMode controlMode = CameraControlMode::GamePad;

    // --- State Variables ---
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 eye = { 0.0f, 5.0f, -10.0f };
    DirectX::XMFLOAT3 fixedPosition = { 0.0f, 15.0f, -10.0f }; // Default position
    DirectX::XMFLOAT3 m_targetOffset = { 0.0f, 0.0f, 0.0f };

    float fixedYawOffset = 0.0f;
    float fixedRollOffset = 0.0f;

    float moveSpeed = 15.0f;
    float rollSpeed = DirectX::XMConvertToRadians(90);
    float range = 10.0f;
    float maxAngleX = DirectX::XMConvertToRadians(85);
    float minAngleX = DirectX::XMConvertToRadians(-85);
    bool toggleCursor = true;
};