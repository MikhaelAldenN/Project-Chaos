#pragma once
#include "IPooledAttackPattern.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackMeteor - Phase 1 attack pattern.
//
// Menembakkan peluru berukuran besar secara berurutan (bergantian) 
// dari sudut kanan atas layar menuju sudut kiri bawah secara diagonal.
// ============================================================

struct MeteorParams {
    int   count = 5;            // Jumlah meteor dalam satu serangan
    float spawnDelay = 0.4f;    // Jeda antar tembakan meteor
    float speed = 35.0f;        // Kecepatan jatuh meteor
    float radius = 1.0f;        // Ukuran hitbox (0.25f normal, 1.0f = besar)
    float visualScale = 4.0f;   // Ukuran visual (setengah dari bounce yang biasanya raksasa)
    float spawnX = 35.0f;       // Posisi awal X (Kanan)
    float spawnZ = 25.0f;       // Posisi awal Z (Atas)
    float spreadOffset = 4.0f;  // Jarak sebar antar meteor agar tidak menumpuk di 1 garis
    float dirX = -1.0f;         // Arah X (ke Kiri)
    float dirZ = -1.0f;         // Arah Z (ke Bawah)
    int   damage = 3;
    float sfxVolume = 1.0f;

    int   triggerCount = 1;     // Untuk AI Combo
    float triggerDelay = 1.0f;
};

class AttackMeteor : public IPooledAttackPattern {
public:
    explicit AttackMeteor(const MeteorParams& params);
    ~AttackMeteor() override = default;

    void StartPooled(Boss* boss, std::vector<std::unique_ptr<Bullet>>* pool) override;
    void Update(float dt, Boss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) override;
    void Stop(Boss* boss) override;

    bool IsFinished() const override;
    std::vector<Bullet*> GetActiveProjectiles() const override { return {}; }

private:
    void FireMeteor();

    MeteorParams                          m_params;
    std::vector<std::unique_ptr<Bullet>>* m_pool = nullptr;
    Boss* m_boss = nullptr;

    bool  m_active = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;

    int   m_currentTrigger = 0;
    float m_triggerTimer = 0.0f;
    bool  m_isTriggerWaiting = false;
};