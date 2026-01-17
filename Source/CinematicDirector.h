#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "CameraController.h"

struct SceneCameraPoint {
    std::string Name;
    DirectX::XMFLOAT3 StartPos;
    DirectX::XMFLOAT3 StartLookAt;
    DirectX::XMFLOAT3 EndPos;
    DirectX::XMFLOAT3 EndLookAt;
    float Duration;
    EasingType Easing;
};

class CinematicDirector
{
public:
    CinematicDirector();

    // Setup data shot (Intro, Angle B, dll)
    void InitializeScenarios();

    // [UPDATE PARAMETER] Kita butuh 'playerPosition' untuk logika FixedFollow di akhir
    void Update(float elapsedTime, float playerEnergy, float thresholdForm, float thresholdDest, DirectX::XMFLOAT3 playerPosition);

    // Helper untuk GUI agar tetap bisa diedit
    std::vector<SceneCameraPoint>& GetScenarios() { return m_camScenarioPoints; }

    // Logic Trigger Manual (Intro)
    void TriggerIntroSequence();

private:
    DirectX::XMFLOAT3 CalculateRotation(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target);

    std::vector<SceneCameraPoint> m_camScenarioPoints;

    // Flags State
    bool m_hasTriggeredFormationCam = false;
    bool m_hasTriggeredDestructionCam = false;
    bool m_isTransitioningToDestruction = false;

    // Internal helper
    void PlaySequence(int index, bool isJumpCut = false);
};