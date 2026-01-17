#include "CinematicDirector.h"
#include "JuiceEngine.h" 
#include <cmath>

using namespace DirectX;

CinematicDirector::CinematicDirector()
{
    InitializeScenarios();
}

void CinematicDirector::InitializeScenarios()
{
    m_camScenarioPoints.clear();

    // SHOT 1: Intro (Index 0)
    m_camScenarioPoints.push_back({
        "Shot 1 (Intro)",
        {0.0f, 20.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
        {0.0f, 14.5f, 0.0f}, {-0.5f, 0.0f, 1.5f},
        3.0f,
        EasingType::EaseInCubic
        });

    // SHOT 2: Angle B (Index 1)
    m_camScenarioPoints.push_back({
        "Shot 2 (Angle B)",
        {1.9f, 12.3f, 6.8f}, {0.0f, 0.0f, 2.1f},
        {-0.4f, 8.5f, 9.3f}, {-0.5f, 0.0f, 1.4f},
        3.0f,
        EasingType::EaseOutCubic
        });

    // SHOT 2.5: Formation (Index 2)
    m_camScenarioPoints.push_back({
        "Shot 2.5 (Formation)",
        {-0.4f, 8.5f, 9.3f},  {-0.5f, 0.0f, 1.4f},
        {-0.4f, 10.5f, 12.0f}, {-0.5f, 0.0f, 1.0f},
        1.5f,
        EasingType::EaseOutCubic
        });

    // SHOT 3: Destruction (Index 3)
    m_camScenarioPoints.push_back({
        "Shot 3 (Destruction)",
        {-0.4f, 8.5f, 9.3f}, {-0.5f, 0.0f, 1.4f}, // START
        {12.2f, 15.0f, 4.3f}, {-1.7f, 0.0f, -4.0f}, // END (Final Position)
        2.5f,
        EasingType::EaseOutCubic
        });
}

// [UPDATE] Sekarang menerima playerPosition
void CinematicDirector::Update(float elapsedTime, float playerEnergy, float thresholdForm, float thresholdDest, DirectX::XMFLOAT3 playerPosition)
{
    // --------------------------------------------------------
    // PHASE 1: FORMATION MODE (Shot 2.5)
    // --------------------------------------------------------
    if (playerEnergy >= thresholdForm && !m_hasTriggeredFormationCam)
    {
        m_hasTriggeredFormationCam = true;

        // Mainkan Index 2 (Formation)
        PlaySequence(2);

        JuiceEngine::Instance().TriggerShake({ 0.5f, 0.2f, 0.5f, 20.0f, 3.0f });
    }

    // --------------------------------------------------------
    // PHASE 2: DESTRUCTION MODE (Shot 3)
    // --------------------------------------------------------
    if (playerEnergy >= thresholdDest && !m_hasTriggeredDestructionCam)
    {
        m_hasTriggeredDestructionCam = true;
        m_isTransitioningToDestruction = true;

        // Mainkan Index 3 (Destruction)
        PlaySequence(3);

        JuiceEngine::Instance().TriggerShake({ 1.5f, 0.8f, 2.0f, 25.0f, 1.0f });
        JuiceEngine::Instance().TriggerGlitchKick(2.0f, 2.0f);
    }

    // --------------------------------------------------------
    // LOGIKA TRANSISI: Sequence -> Fixed Follow
    // --------------------------------------------------------
    if (m_isTransitioningToDestruction)
    {
        // Cek apakah Sequence Kamera SUDAH BERHENTI (artinya sudah sampai tujuan)
        if (!CameraController::Instance().IsSequencing())
        {
            m_isTransitioningToDestruction = false;

            // Pastikan data Shot 3 ada
            if (m_camScenarioPoints.size() > 3)
            {
                const auto& shotDest = m_camScenarioPoints[3];
                auto& camCtrl = CameraController::Instance();

                // 1. Hitung Offset Posisi (Jarak Kamera ke Player)
                XMFLOAT3 posOffset;
                posOffset.x = shotDest.EndPos.x - playerPosition.x;
                posOffset.y = shotDest.EndPos.y - playerPosition.y;
                posOffset.z = shotDest.EndPos.z - playerPosition.z;

                // 2. Hitung Offset Pandangan (LookAt relatif ke Player)
                XMFLOAT3 lookOffset;
                lookOffset.x = shotDest.EndLookAt.x - playerPosition.x;
                lookOffset.y = shotDest.EndLookAt.y - playerPosition.y;
                lookOffset.z = shotDest.EndLookAt.z - playerPosition.z;

                // 3. Set Mode ke FIXED FOLLOW
                camCtrl.SetControlMode(CameraControlMode::FixedFollow);
                camCtrl.SetFixedSetting(posOffset);   // Jarak Kamera
                camCtrl.SetTargetOffset(lookOffset);  // Offset Pandangan
                camCtrl.SetTarget(playerPosition);    // Pusatnya Player
            }
        }
    }
}

void CinematicDirector::TriggerIntroSequence()
{
    if (m_camScenarioPoints.empty()) return;

    std::vector<CameraKeyframe> sequence;
    int introShotCount = 2; // Shot 1 & Shot 2

    for (int i = 0; i < m_camScenarioPoints.size(); ++i)
    {
        if (i >= introShotCount) break;

        const auto& point = m_camScenarioPoints[i];
        CameraKeyframe key;

        key.isJumpCut = true; // Intro pakai Jump Cut

        key.StartPosition = point.StartPos;
        key.StartRotation = CalculateRotation(point.StartPos, point.StartLookAt);

        key.TargetPosition = point.EndPos;
        key.TargetRotation = CalculateRotation(point.EndPos, point.EndLookAt);

        key.Duration = point.Duration;
        key.Easing = point.Easing;

        sequence.push_back(key);
    }

    CameraController::Instance().PlaySequence(sequence, false);
}

void CinematicDirector::PlaySequence(int index, bool isJumpCut)
{
    if (index < 0 || index >= m_camScenarioPoints.size()) return;

    const auto& shot = m_camScenarioPoints[index];
    std::vector<CameraKeyframe> seq;
    CameraKeyframe key;

    key.isJumpCut = isJumpCut;
    key.StartPosition = shot.StartPos; // Penting jika isJumpCut = true
    key.StartRotation = CalculateRotation(shot.StartPos, shot.StartLookAt);

    key.TargetPosition = shot.EndPos;
    key.TargetRotation = CalculateRotation(shot.EndPos, shot.EndLookAt);
    key.Duration = shot.Duration;
    key.Easing = shot.Easing;

    seq.push_back(key);

    CameraController::Instance().PlaySequence(seq, false);
}

// Math Helper: Konversi (Posisi + Target) menjadi (Pitch, Yaw)
DirectX::XMFLOAT3 CinematicDirector::CalculateRotation(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target)
{
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vTarget = XMLoadFloat3(&target);
    XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
    vDir = XMVector3Normalize(vDir);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);

    float pitch = asinf(-dir.y);
    float yaw = atan2f(dir.x, dir.z);

    return { pitch, yaw, 0.0f };
}