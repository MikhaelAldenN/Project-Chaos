#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include <vector>
#include <memory>

class Player;

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

    // ==========================================
    // [NEW] Parameter Rhythm Laser
    // ==========================================
    float laserDuration = 2.0f;     // Waktu sampai lingkaran besar menyatu ke lingkaran kecil
    float laserParryWindow = 0.15f; // Jendela toleransi (margin error) = 0.15 detik
    float laserStartRadius = 8.0f;  // Ukuran awal lingkaran besar
    float laserTargetRadius = 1.5f; // Ukuran lingkaran kecil (Target)
    int laserDamage = 20;           // Damage jika gagal parry
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
    void TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos);
    void TriggerLockingLaser(Player* targetPlayer);

    NaviBulletParams& GetParams() { return m_params; }

    std::vector<std::unique_ptr<Bullet>>& GetProjectiles() { return m_bulletPool; }

    float GetLaserTimer() const { return m_laserTimer; }

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

    // ==========================================
    // [NEW] State Rhythm Laser
    // ==========================================
    bool m_isLaserLocked = false;
    float m_laserTimer = 0.0f;
    Player* m_laserTargetPlayer = nullptr; // Pointer aman karena Player dikelola SceneBoss

};