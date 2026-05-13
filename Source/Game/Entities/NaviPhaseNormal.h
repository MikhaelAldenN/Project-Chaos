#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include <vector>
#include <memory>

class Player;

// --- [CENTRALIZED PARAMETERS] ---
struct NaviBulletParams {
    float screenDespawnPadding = 5.0f;

    // Parameter General & Radial Burst
    float speed = 14.0f;
    int count = 24;
    float fireRate = 1.5f;
    float burstDelay = 0.25f;
    DirectX::XMFLOAT4 color = { 1.0f, 0.2f, 0.2f, 1.0f };
    float despawnDist = 60.0f;

    // Parameter Targeted Fan Burst (Shotgun)
    int fanLines = 4;
    int fanWaves = 5;
    float fanWaveDelay = 0.15f;
    float fanSpreadAngle = 0.2f;

    // ==========================================
    // Parameter Rhythm Laser
    // ==========================================
    float laserDuration = 2.0f;
    float laserParryWindow = 0.15f;
    float laserStartRadius = 8.0f;
    float laserTargetRadius = 1.5f;
    int laserDamage = 20;

    // ==========================================
    // Parameter Bijuudama
    // ==========================================
    DirectX::XMFLOAT4 bijuudamaColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // Merah Darah!
    float bijuudamaBaseHitbox = 0.5f;
    float bijuudamaMaxHitboxGrow = 2.0f;
    float bijuudamaVisualMultiplier = 4.0f;
    float bijuudamaSpawnOffsetZ = 2.0f;
    float bijuudamaShootSpeed = 45.0f;

    // ==========================================
    // [BARU] Parameter Shatter Bijuudama
    // ==========================================
    int shatterMinFragments = 3;
    int shatterMaxFragments = 5;
    float shatterMinRadius = 0.2f;
    float shatterMaxRadius = 0.8f;
    float shatterMinDuration = 0.5f; // Waktu tempuh (Lebih besar = Lebih lambat)
    float shatterMaxDuration = 0.7f;
    float shatterCurveOffset = 11.0f; // Seberapa melengkung / lebar parabolanya};

    // ==========================================
    // Parameter Glintstone
    // ==========================================
    int phalanxCount = 5;               // Jumlah pedang/peluru
    float phalanxChargeDelay = 0.2f;    // Waktu panggil tiap peluru
    float phalanxFireDelay = 0.3f;      // Waktu jeda antar tembakan
    float phalanxSpeed = 35.0f;         // Kecepatan terbang
    float phalanxHoverRadius = 4.0f;    // Jarak melayang di sekitar bos
    float phalanxTurnSpeed = 1.5f;      // Homing LEMAH (Supaya player bisa dash!)

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
    void TriggerBijuudama(Player* targetPlayer);
    void CancelBijuudama() {
        m_isLaserLocked = false;
        m_bijuudamaBall = nullptr;
    }

    void TriggerPhalanx(Player* targetPlayer);

    void ShatterBijuudama(DirectX::XMFLOAT3 parryPos, NaviBoss* boss);

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

    Bullet* m_bijuudamaBall = nullptr;

    // ==========================================
    // [NEW] State Glintstone Phalanx
    // ==========================================
    int m_phalanxState = 0; // 0=Mati, 1=Charging/Panggil, 2=Nembak
    float m_phalanxTimer = 0.0f;
    int m_phalanxSpawned = 0;
    int m_phalanxFired = 0;
    Player* m_phalanxTarget = nullptr;
    std::vector<Bullet*> m_phalanxBullets; // Pegang peluru yang sedang melayang
};