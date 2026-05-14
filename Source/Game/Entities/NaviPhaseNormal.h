#pragma once
#include "INaviPhase.h"
#include "Bullet.h"
#include <vector>
#include <memory>
#include "Primitive.h"

class Player;

// --- [CENTRALIZED PARAMETERS] ---
struct NaviBulletParams {
    float screenDespawnPadding = 5.0f;

    // Parameter General & Radial Burst
    float speed = 20.0f;
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
    float laserDuration = 4.0f;
    float laserParryWindow = 0.25f;
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
    float bijuudamaAttackMoveSpeed = 5.0f; // Kecepatan meluncur ke atas
    float bijuudamaReturnMoveSpeed = 2.0f; // Kecepatan balik ke tengah (lebih anggun)
    float bijuudamaPostFireDelay = 1.0f;

    // ==========================================
    // Parameter Shatter Bijuudama
    // ==========================================
    int shatterMinFragments = 5;
    int shatterMaxFragments = 8;
    float shatterMinRadius = 0.2f;
    float shatterMaxRadius = 0.8f;
    float shatterMinDuration = 0.5f; // Waktu tempuh (Lebih besar = Lebih lambat)
    float shatterMaxDuration = 0.7f;
    float shatterCurveOffset = 11.0f; // Seberapa melengkung / lebar parabolanya};

    // ==========================================
    // Parameter Glintstone
    // ==========================================
    int phalanxCount = 7;               // Jumlah pedang/peluru
    float phalanxChargeDelay = 0.2f;    // Waktu panggil tiap peluru
    float phalanxHoldDuration = 2.0f;
    float phalanxFireDelay = 0.0f;      // Waktu jeda antar tembakan
    float phalanxSpeed = 80.0f;         // Kecepatan terbang
    float phalanxHoverRadius = 4.0f;    // Jarak melayang di sekitar bos
    float phalanxTurnSpeed = 1.5f;      // Homing LEMAH (Supaya player bisa dash!)
    float phalanxSmoothSpeed = 12.0f;

    float phalanxPostFireDelay = 1.0f;    // Delay 1 detik setelah tembakan habis
    float phalanxAttackMoveSpeed = 2.5f;  // Speed saat meluncur ke samping
    float phalanxReturnMoveSpeed = 2.5f;  // Speed saat pulang ke tengah (lebih lambat)

    // ==========================================
    // Parameter Asgore Rain (Area Denial)
    // ==========================================
    float rainWarningDuration = 1.5f;   // Berapa lama tanda bahaya muncul
    float rainActiveDuration = 2.0f;    // Berapa lama hujannya berlangsung
    float rainWidth = 25.0f;            // Lebar area (Sumbu X)
    float rainDepth = 40.0f;            // Panjang area (Sumbu Z)
    int rainDamage = 1;                // Damage jika masuk area
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
    void TriggerTripleBurst();
    void TriggerFanAttack(NaviBoss* boss, DirectX::XMFLOAT3 playerPos);
    void TriggerBijuudama(Player* targetPlayer);
    void CancelBijuudama() {
        m_isLaserLocked = false;
        m_bijuudamaBall = nullptr;
        m_laserTimer = 0.0f;
        m_isBijuudamaRecovering = true;
        m_bijuudamaRecoveryTimer = 0.0f;
    }
    bool IsLaserLocked() const { return m_isLaserLocked; }

    void TriggerPhalanx(Player* targetPlayer);
    void TriggerRainAttack(bool isVertical, bool isPositiveSide, float sweepDir = 1.0f, bool isDual = false);
    void ShatterBijuudama(DirectX::XMFLOAT3 parryPos, NaviBoss* boss);
    int GetRainState() const { return m_rainState; }
    bool IsRainVertical() const { return m_rainIsVertical; }
    bool IsRainDual() const { return m_rainIsDual; }
    DirectX::XMFLOAT3 GetRainCenter2() const { return m_rainCenter2; }

    float GetActualRainWidth() const {
        if (m_rainIsDual) return 18.0f;
        return m_rainIsVertical ? 25.0f : 80.0f;
    }
    float GetActualRainDepth() const { return m_rainIsVertical ? 45.0f : 15.0f; }

    DirectX::XMFLOAT3 GetRainCenter() const { return m_rainCenter; }

    NaviBulletParams& GetParams() { return m_params; }

    std::vector<std::unique_ptr<Bullet>>& GetProjectiles() { return m_bulletPool; }

    float GetLaserTimer() const { return m_laserTimer; }

    // ==========================================
    // Sistem Health Boss
    // ==========================================
    void TakeDamage(int damage);
    int GetHP() const { return m_bossHP; }
    int GetMaxHP() const { return m_bossMaxHP; }
    bool IsDead() const { return m_bossHP <= 0; }

    // ==========================================
    // [NEW] AI Director (Omega Flowey Mode)
    // ==========================================
    void SetAITarget(Player* p) { m_aiTarget = p; }
    void SetAIEnabled(bool val) { m_aiEnabled = val; }
    bool IsAIEnabled() const { return m_aiEnabled; }

private:
    void FireRadialBurst(NaviBoss* boss, float angleOffset);
    void FireFanWave(NaviBoss* boss); // Fungsi tembak fan/shotgun
    void UpdateAI(float dt, NaviBoss* boss); // Fungsi Otak Utama

private:
    std::vector<std::unique_ptr<Bullet>> m_bulletPool;
    NaviBulletParams m_params;

    // Sequence States: Radial
    bool m_isFiring = false;
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
    bool m_isBijuudamaRecovering = false;
    float m_bijuudamaRecoveryTimer = 0.0f;

    // ==========================================
    // [NEW] State Glintstone Phalanx
    // ==========================================
    int m_phalanxState = 0; //0 = Mati, 1 = Charging, 2 = Holding, 3 = Firing
    float m_phalanxTimer = 0.0f;
    int m_phalanxSpawned = 0;
    int m_phalanxFired = 0;
    Player* m_phalanxTarget = nullptr;
    std::vector<Bullet*> m_phalanxBullets; // Pegang peluru yang sedang melayang

    // ==========================================
    // [NEW] State Asgore Rain
    // ==========================================
    int m_rainState = 0; // 0 = Off, 1 = Warning, 2 = Raining
    float m_rainTimer = 0.0f;
    DirectX::XMFLOAT3 m_rainCenter = { 0,0,0 };
    bool m_rainIsVertical = false;
    float m_rainSweepDir = 1.0f;

    std::unique_ptr<Primitive> m_zonePrimitive;

    int m_bossMaxHP = 1000;
    int m_bossHP = 1000;
    float m_hitFlashTimer = 0.0f; // Untuk efek kedip saat kena tembak

    bool m_rainIsDual = false;
    DirectX::XMFLOAT3 m_rainCenter2 = { 0,0,0 };

    // ==========================================
    // [NEW] AI Cooldown Memory
    // ==========================================
    bool m_aiEnabled = false;
    Player* m_aiTarget = nullptr;

    // Timer awal sebelum serangan pertama diluncurkan
    float m_cdRadial = 2.0f;
    float m_cdFan = 4.0f;
    float m_cdPhalanx = 6.0f;
    float m_cdRain = 10.0f;
    float m_cdBijuudama = 15.0f;

    // [BARU] Waktu istirahat mutlak antar serangan!
    float m_aiGlobalCooldown = 0.5f;

    // ==========================================
    // [NEW] Sistem Pergerakan AI
    // ==========================================
    DirectX::XMFLOAT3 m_targetPosition = { 0.0f, 0.0f, 0.0f };
    float m_moveLerpSpeed = 3.5f; // Kecepatan jendela mengejar target
    float m_currentMoveLerpSpeed = 0.0f;
    float m_moveAcceleration = 8.0f;
};