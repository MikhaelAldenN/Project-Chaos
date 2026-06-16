#pragma execution_character_set("utf-8")
#include "AttackMeteor.h"
#include "Boss.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

AttackMeteor::AttackMeteor(const MeteorParams& params)
    : m_params(params) {}

void AttackMeteor::StartPooled(Boss* boss, std::vector<std::unique_ptr<Bullet>>* pool) {
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_spawnedCount = 0;
    m_currentTrigger = 0;
    m_isTriggerWaiting = false;
    m_triggerTimer = 0.0f;

    // Set agar meteor pertama langsung meluncur
    m_spawnTimer = m_params.spawnDelay;
}

void AttackMeteor::Update(float dt, Boss* boss) {
    if (!m_active || !m_pool) return;

    if (m_isTriggerWaiting) {
        m_triggerTimer += dt;
        if (m_triggerTimer >= m_params.triggerDelay) {
            m_isTriggerWaiting = false;
            m_spawnedCount = 0;
            m_spawnTimer = m_params.spawnDelay;
        }
        return;
    }

    m_spawnTimer += dt;
    if (m_spawnTimer >= m_params.spawnDelay) {
        m_spawnTimer -= m_params.spawnDelay;

        FireMeteor();

        if (++m_spawnedCount >= m_params.count) {
            m_currentTrigger++;
            if (m_currentTrigger >= m_params.triggerCount) {
                m_active = false;
            }
            else {
                m_isTriggerWaiting = true;
                m_triggerTimer = 0.0f;
            }
        }
    }
}

void AttackMeteor::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    // Peluru dirender terpusat oleh Phase
}

void AttackMeteor::Stop(Boss* boss) {
    m_active = false;
}

bool AttackMeteor::IsFinished() const {
    return !m_active;
}

void AttackMeteor::FireMeteor() {
    if (!m_pool) return;

    for (auto& bullet : *m_pool) {
        if (!bullet->IsActive()) {

            float length = sqrtf((m_params.dirX * m_params.dirX) + (m_params.dirZ * m_params.dirZ));
            float normX = m_params.dirX / length;
            float normZ = m_params.dirZ / length;

            // ========================================================
            // LOGIKA SCATTER (MENYEBAR SELURUH LAYAR & TIDAK URUT)
            // ========================================================

            // 1. Prime Jump: Gunakan bilangan prima (misal 7) agar indeks melompat secara "acak"
            // Namun urutannya 100% SAMA (deterministik) setiap kali boss menggunakan serangan ini!
            int primeJump = 7;
            int scatterIndex = (m_spawnedCount * primeJump) % m_params.count;

            // 2. Ubah indeks menjadi persentase posisi dari 0.0 (Kanan) ke 1.0 (Kiri)
            float fraction = (m_params.count > 1) ? (float)scatterIndex / (m_params.count - 1) : 0.5f;

            // 3. Rentangkan posisi X layar dari Kanan (spawnX) hingga Kiri.
            // Kita kurangi batas kirinya (+ 10.0f) agar peluru paling kiri tidak langsung hilang ditebas Garbage Collector
            float startX = m_params.spawnX;               // Pojok Kanan Atas
            float endX = -m_params.spawnX + 10.0f;        // Kiri Atas

            // 4. Hitung X final berdasarkan urutan acak yang didapat
            float xPos = startX - (fraction * (startX - endX));

            DirectX::XMFLOAT3 spawnPos = {
                xPos,
                1.0f,
                m_params.spawnZ // Z tetap sama (selalu mulai dari atas layar)
            };

            DirectX::XMFLOAT3 direction = { normX, 0.0f, normZ };

            bullet->SetRadius(m_params.radius);
            bullet->scale = { m_params.visualScale, m_params.visualScale, m_params.visualScale };
            bullet->SetHomingTarget(nullptr);
            bullet->SetBossTarget(nullptr);
            bullet->SetParabolic(false);
            bullet->SetParryReturn(false);
            bullet->SetTurnSpeed(8.0f);
            bullet->SetDamage(m_params.damage);

            bullet->Fire(spawnPos, direction, m_params.speed);
            break;
        }
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}