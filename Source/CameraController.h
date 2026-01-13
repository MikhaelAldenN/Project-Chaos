#pragma once
#include <DirectXMath.h>
#include <functional>

class Camera;

// Tambahkan Enum untuk Easing agar fleksibel
enum class EasingType
{
    Linear,
    EaseInQuad,
    EaseOutQuad,
    SmoothStep
};

enum class CameraControlMode
{
    GamePad,
    Mouse,
    FixedFollow,
    FixedStatic,
    Free,
    Transition // Mode khusus saat kamera sedang blending
};

class CameraController
{
public:
    static CameraController& Instance();

    void SetActiveCamera(Camera* camera);
    Camera* GetActiveCamera() const { return activeCamera; }

    void Update(float elapsedTime);
    void DrawDebugGUI();

    // --- Basic Controls ---
    void SetTarget(const DirectX::XMFLOAT3& target);
    void SetTargetOffset(const DirectX::XMFLOAT3& offset) { m_targetOffset = offset; }
    void SetFixedSetting(const DirectX::XMFLOAT3& value);
    void SetFixedYawOffset(float radians) { fixedYawOffset = radians; }
    void SetFixedRollOffset(float radians) { fixedRollOffset = radians; }
    void SetControlMode(CameraControlMode mode);
    CameraControlMode GetControlMode() const { return controlMode; }
    void ClearCamera() { activeCamera = nullptr; }

    // --- NEW: Transition System ---
    // Fungsi ini akan memulai blending dari posisi kamera saat ini ke target kamera
    void StartTransition(Camera* targetCamSettings, float duration, EasingType easing = EasingType::SmoothStep);

    // Cek apakah sedang transisi
    bool IsTransitioning() const { return controlMode == CameraControlMode::Transition; }

private:
    CameraController();
    ~CameraController();
    CameraController(const CameraController&) = delete;
    void operator=(const CameraController&) = delete;

    // Helper Math
    float ApplyEasing(float t, EasingType type);

    Camera* activeCamera = nullptr;
    CameraControlMode controlMode = CameraControlMode::GamePad;
    CameraControlMode previousMode = CameraControlMode::FixedStatic; // Mengingat mode sebelum transisi

    // --- State Variables ---
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 eye = { 0.0f, 5.0f, -10.0f };
    DirectX::XMFLOAT3 fixedPosition = { 0.0f, 15.0f, -10.0f };
    DirectX::XMFLOAT3 m_targetOffset = { 0.0f, 0.0f, 0.0f };
    float fixedYawOffset = 0.0f;
    float fixedRollOffset = 0.0f;
    float moveSpeed = 15.0f;
    float rollSpeed = DirectX::XMConvertToRadians(90);
    float range = 10.0f;
    float maxAngleX = DirectX::XMConvertToRadians(85);
    float minAngleX = DirectX::XMConvertToRadians(-85);
    bool toggleCursor = true;

    // --- Transition Data ---
    struct TransitionState
    {
        DirectX::XMFLOAT3 startPos;
        DirectX::XMFLOAT4 startRotQuat; // Menggunakan Quaternion untuk rotasi yang mulus

        DirectX::XMFLOAT3 endPos;
        DirectX::XMFLOAT4 endRotQuat;

        float currentTime = 0.0f;
        float duration = 1.0f;
        EasingType easingType = EasingType::SmoothStep;
        bool active = false;
    } transition;
};