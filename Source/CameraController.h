#pragma once
#include <DirectXMath.h>

// Forward declaration to reduce compile dependencies
class Camera;

enum class CameraControlMode
{
    GamePad,
    Mouse,
    FixedFollow,
    FixedStatic,
    Free
};

class CameraController
{
public:
    // Singleton Access
    static CameraController& Instance();

    // Set the camera object that this controller will manipulate
    void SetActiveCamera(Camera* camera);

    Camera* GetActiveCamera() const { return activeCamera; }

    // Main logic loop
    void Update(float elapsedTime);

    // ImGui debug window
    void DrawDebugGUI();

    // Set the target/focus point for the camera (e.g., Player position)
    void SetTarget(const DirectX::XMFLOAT3& target);
    void SetTargetOffset(const DirectX::XMFLOAT3& offset) { m_targetOffset = offset; }

    // Set fixed Static 
    void SetFixedSetting(const DirectX::XMFLOAT3& value);
    void SetFixedYawOffset(float radians) { fixedYawOffset = radians; }
    float GetFixedYawOffset() const { return fixedYawOffset; }

    // Switch control modes
    void SetControlMode(CameraControlMode mode);

    CameraControlMode GetControlMode() const { return controlMode; }

    void ClearCamera() { activeCamera = nullptr; }

private:
    CameraController();
    ~CameraController();

    // Prevent copying
    CameraController(const CameraController&) = delete;
    void operator=(const CameraController&) = delete;

    // --- State Variables ---
    Camera* activeCamera = nullptr;
    CameraControlMode controlMode = CameraControlMode::GamePad;

    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };

    // Free Camera / Debug settings
    DirectX::XMFLOAT3 eye = { 0.0f, 5.0f, -10.0f };
    float moveSpeed = 15.0f;
    float rollSpeed = DirectX::XMConvertToRadians(90);

    // Static Fixed setting 
    float fixedYawOffset = 0.0f;

    // Orbit Camera settings
    float range = 10.0f;
    float maxAngleX = DirectX::XMConvertToRadians(85);
    float minAngleX = DirectX::XMConvertToRadians(-85);

    // Fixed Camera settings
    DirectX::XMFLOAT3 fixedPosition = { 0.0f, 15.0f, -10.0f };

    DirectX::XMFLOAT3 m_targetOffset = { 0.0f, 0.0f, 0.0f };
    // UI State
    bool toggleCursor = true;
};