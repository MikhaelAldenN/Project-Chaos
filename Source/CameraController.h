#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <functional> // Untuk callback

class Camera;

// Enum untuk tipe transisi agar gerakan lebih natural
enum class EasingType {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInCubic,  // <-- BARU: Lebih lambat di awal dibanding Quad
    EaseOutCubic, // <-- BARU: Lebih lambat berhenti dibanding Quad
    SmoothStep
};

// Struct untuk menyimpan data "Waypoint" kamera
struct CameraKeyframe {
    DirectX::XMFLOAT3 TargetPosition;
    DirectX::XMFLOAT3 TargetRotation; // Euler Angles (Radians)
    float Duration;                   // Waktu tempuh ke titik ini (detik)
    EasingType Easing = EasingType::SmoothStep;
};

enum class CameraControlMode
{
    GamePad,
    Mouse,
    FixedFollow,
    FixedStatic,
    Free,
    Sequence // MODE BARU: Sedang menjalankan cutscene/urutan
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

    void SetSplineTension(float tension) { m_splineTension = tension; }
    float GetSplineTension() const { return m_splineTension; }

    void SetControlMode(CameraControlMode mode);
    CameraControlMode GetControlMode() const { return controlMode; }
    void ClearCamera() { m_activeCamera.reset(); }

    // --- NEW: Sequence / Transition System ---
    // Memulai urutan gerakan baru (akan menghapus antrian lama)
    void PlaySequence(const std::vector<CameraKeyframe>& sequence, bool loop = false);
    void PlaySequenceBySpeed(const std::vector<CameraKeyframe>& targets, float speed, EasingType globalEasing, bool useSplinePath, bool loop = false);
    void PlaySmoothPath(const std::vector<CameraKeyframe>& points, float totalDuration, EasingType globalEasing = EasingType::SmoothStep);

    // Menambahkan keyframe ke antrian yang sedang berjalan
    void AppendKeyframe(const CameraKeyframe& keyframe);

    // Menghentikan sequence secara paksa
    void StopSequence();

    // Cek apakah sequence sedang berjalan
    bool IsSequencing() const { return controlMode == CameraControlMode::Sequence; }

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
    DirectX::XMFLOAT3 fixedPosition = { 0.0f, 15.0f, -10.0f };
    DirectX::XMFLOAT3 m_targetOffset = { 0.0f, 0.0f, 0.0f };

    float fixedYawOffset = 0.0f;
    float fixedRollOffset = 0.0f;

    float m_splineTension = 1.0f;

    float moveSpeed = 15.0f;
    float rollSpeed = DirectX::XMConvertToRadians(90);
    float range = 10.0f;
    float maxAngleX = DirectX::XMConvertToRadians(85);
    float minAngleX = DirectX::XMConvertToRadians(-85);
    bool toggleCursor = true;

    // --- Sequence Internal State ---
    std::vector<CameraKeyframe> m_sequenceQueue;
    int m_currentKeyframeIdx = 0;
    float m_seqTimer = 0.0f;
    bool m_seqLoop = false;
    bool m_useSpline = false;

    // Cache posisi awal sebelum bergerak ke keyframe berikutnya
    DirectX::XMFLOAT3 m_seqStartPos;
    DirectX::XMFLOAT3 m_seqStartRot;

    // --- State untuk Smooth Path ---
    bool m_isSmoothPath = false;
    float m_smoothTotalDuration = 0.0f;
    EasingType m_smoothEasing = EasingType::SmoothStep;
    std::vector<DirectX::XMFLOAT3> m_pathPoints; // Hanya butuh posisi untuk kalkulasi kurva

    // Helper Math: Catmull-Rom Spline (Standard industri untuk jalur kamera halus)
    DirectX::XMFLOAT3 CatmullRom(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& p3, float t);
    DirectX::XMFLOAT3 HermiteSpline(const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& t1, const DirectX::XMFLOAT3& t2, float t);

    // Helper Functions
    float ApplyEasing(float t, EasingType type);
    DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float t);
    DirectX::XMFLOAT3 LerpAngles(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float t);
};