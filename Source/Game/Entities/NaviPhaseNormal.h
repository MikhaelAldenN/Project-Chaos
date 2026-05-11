#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include <vector>
#include <memory>

// --- [CENTRALIZED PARAMETERS] ---
struct NaviBulletParams {
    // Parameter General & Radial Burst
    float speed = 14.0f;
    int count = 24;
    float fireRate = 1.5f;
    float burstDelay = 0.25f;
    DirectX::XMFLOAT4 color = { 1.0f, 0.2f, 0.2f, 1.0f };
    float despawnDist = 60.0f;

    // Parameter Targeted Fan Burst (Shotgun)
    int fanLines = 4;           // Jumlah peluru per baris/wave
    int fanWaves = 5;           // Ditembakkan berapa kali
    float fanWaveDelay = 0.15f; // Jeda waktu antar wave (detik)
    float fanSpreadAngle = 0.2f;// Jarak sudut antar peluru (radian)
};

class NaviPhaseNormal : public INaviPhase {
public:
    NaviPhaseNormal() = default;
    ~NaviPhaseNormal() override = default;

    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // --- Attack Triggers ---
    void TriggerSingleBurst();
    void TriggerDoubleBurst();

    // BARU: Membutuhkan Boss dan Posisi Player untuk mengunci target
    void TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos);

    NaviBulletParams& GetParams() { return m_params; }

private:
    void FireRadialBurst(NaviBoss* boss, float angleOffset);
    void FireFanWave(NaviBoss* boss); // Fungsi tembak fan/shotgun

private:
    std::vector<std::unique_ptr<Bullet>> m_bulletPool;
    NaviBulletParams m_params;

    // Sequence States: Radial
    bool m_isFiring = false;
    bool m_isDoubleBurst = false;
    int m_burstsFired = 0;
    float m_burstSequenceTimer = 0.0f;

    // Sequence States: Fan Burst
    bool m_isFiringFan = false;
    int m_fanWavesFired = 0;
    float m_fanTimer = 0.0f;
    float m_lockedBaseAngle = 0.0f; // Menyimpan posisi terakhir player
};