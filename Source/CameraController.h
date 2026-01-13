#pragma once
#include <DirectXMath.h>
#include <functional>
#include <vector> // [BARU] Wajib include ini
#include <memory>

class Camera;

// Enum Easing
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
    Transition
};

struct CameraKeyframe
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation; // Euler Angles (Pitch, Yaw, Roll)
    DirectX::XMFLOAT3 TargetLookAt; // Opsional, jika ingin transisi fokus

    // [BARU] Default Constructor (Penting agar tidak error E0291)
    // Ini mengizinkan pembuatan variable: "CameraKeyframe startFrame;"
    CameraKeyframe()
        : Position({ 0,0,0 }), Rotation({ 0,0,0 }), TargetLookAt({ 0,0,0 }) {}

    // Constructor helper yang sudah ada
    CameraKeyframe(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 rot)
        : Position(pos), Rotation(rot), TargetLookAt({ 0,0,0 }) {}
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
    void SetFixedYawOffset(float radians) { fixedYawOffset = radians; }
    void SetFixedRollOffset(float radians) { fixedRollOffset = radians; }
    void SetControlMode(CameraControlMode mode);
    CameraControlMode GetControlMode() const { return controlMode; }
    void ClearCamera() { m_activeCamera.reset(); }

    // --- NEW: Sequence System ---
    // Fungsi lama (StartTransition) bisa kita overload atau hapus, tapi untuk kompatibilitas
    // kita biarkan, tapi di dalamnya nanti memanggil sistem path.
    void StartTransition(Camera* targetCamSettings, float duration, EasingType easing = EasingType::SmoothStep);

    // [BARU] Fungsi Utama: Menerima daftar kamera (Jalur)
    void StartPathTransition(const std::vector<CameraKeyframe>& keyframes, float duration, EasingType easing = EasingType::SmoothStep);
    bool IsTransitioning() const { return controlMode == CameraControlMode::Transition; }

private:
    CameraController();
    ~CameraController();
    CameraController(const CameraController&) = delete;
    void operator=(const CameraController&) = delete;

    float ApplyEasing(float t, EasingType type);

    std::weak_ptr<Camera> m_activeCamera;
    CameraControlMode controlMode = CameraControlMode::GamePad;
    CameraControlMode previousMode = CameraControlMode::FixedStatic;

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

    // --- [UPDATE] Transition Data ---
    struct TransitionState
    {
        // Struct kecil untuk menyimpan snapshot data setiap keyframe
        struct KeyframeData {
            DirectX::XMFLOAT3 pos;
            DirectX::XMFLOAT4 rotQuat; // Quaternion
            float roll;                // Roll disimpan terpisah
        };

        // List Keyframe (Jalur Kamera)
        std::vector<KeyframeData> keyframes;

        float currentTime = 0.0f;
        float duration = 1.0f;
        EasingType easingType = EasingType::SmoothStep;
        bool active = false;

        // Data target akhir (untuk setup FixedStatic setelah selesai)
        DirectX::XMFLOAT3 finalTargetLookAt;
    } transition;
};