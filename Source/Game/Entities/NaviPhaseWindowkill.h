#pragma once
#include "INaviPhase.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "BeyondWindow.h"
#include "Camera.h"
#include "Bullet.h"
#include "Primitive.h"
#include "EffectManager.h"
#include "UIDialogueBox.h"

class Sprite;
class Player;

// Pindahkan struktur sayap ke sini, karena hanya Fase Windowkill yang butuh ini!
struct WingNode {
    DirectX::XMFLOAT2 localOffset;
    DirectX::XMFLOAT2 targetOffset;
    DirectX::XMFLOAT2 size;
    float flapOffset;
    float spawnDelay = 0.0f;
    float animScale = 0.0f;
    bool isClosing = false;
};

struct BouncingWindowBullet {
    std::unique_ptr<Bullet> bullet;
    std::string windowName;
    int bounceCount = 0;
    int maxBounces = 8;
};

struct BoomerangWindowBullet {
    std::unique_ptr<Bullet> bullet;
    std::string windowName;
    int state = 0;         // 0 = Masuk ke layar, 1 = Putar balik
    int spawnSide = 1;     // 1 = Kanan, -1 = Kiri
    float startX = 0.0f;   // Titik awal offscreen
    float targetX = 0.0f;  // Titik putar balik (2/3 layar)]
    float targetVelX = 0.0f;
};

// =========================================================
// UNDYNE SPEAR STRUCT & PARAMS
// =========================================================
struct UndyneSpearParams {
    int   count = 10;                // Parameter: Spawn Count
    float spawnDelay = 0.4f;        // Parameter: Spawn Delay
    float hoverDuration = 1.0f;     // Parameter: Waktu Membidik
    float telegraphDuration = 1.0f; // Parameter: Shoot Delay (Laser menyala)
    float startSpeed = 10.0f;
    float maxSpeed = 60.0f;        // Parameter: Max Speed
    float acceleration = 10.0f;
    int   damage = 10;              // Parameter: Spear Damage
    float arcRadius = 20.0f;      // Jari-jari lengkungan busur
    float arcCenterX = 0.0f;      // Posisi X titik pusat (0 = Tengah horizontal)
    float arcCenterZ = -15.0f;    // Posisi Z titik pusat (-35 = Bawah arena/layar)
    float arcMinAngle = 20.0f;    // Sudut minimal (Derajat)
    float arcMaxAngle = 160.0f;   // Sudut maksimal (Derajat)
};

// =========================================================
// [BARU] Parameter Khusus Targeted Blaster
// =========================================================
struct TargetedBlasterParams {
    int   spawnCount = 8;
    float spawnDelay = 0.8f;      // Jeda antar kemunculan meriam
    float dropInDuration = 0.2f;  // Meriam jatuh lebih cepat dari atas
    float chargeDelay = 0.2f;     // Waktu peringatan (Laser transparan) sebelum nembak!
    float fireDuration = 1.0f;
    float fixedTargetZ = 10.0f;   // Sumbu Z tetap agar sejajar dengan blaster acak
    float beamHitboxWidth = 3.0f;
    int   beamDamage = 20;
};

class NaviPhaseWindowkill : public INaviPhase {
public:
    NaviPhaseWindowkill(Player* player = nullptr);
    ~NaviPhaseWindowkill() override = default;

    // --- Kontrak Wajib dari INaviPhase ---
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // --- Custom Functions untuk Fase Ini (Dipanggil oleh ImGui) ---
    void ReplayAnimation();

    void SetWingFlapParams(float speed, float intensity) { m_wingFlapSpeed = speed; m_wingFlapIntensity = intensity; }
    float GetWingFlapSpeed() const { return m_wingFlapSpeed; }
    float GetWingFlapIntensity() const { return m_wingFlapIntensity; }

    void SetWingOffsets(float xOffset, float zOffset) { m_wingXOffset = xOffset; m_wingZOffset = zOffset; }
    float GetWingOffsetX() const { return m_wingXOffset; }
    float GetWingOffsetZ() const { return m_wingZOffset; }

    void SetWingSeed(unsigned int seed) {
        if (m_wingSeed != seed) { m_wingSeed = seed; GenerateButterflyWings(); }
    }
    unsigned int GetWingSeed() const { return m_wingSeed; }

    void SetSpawnParams(float popDur, float spawnDur, float chaos) {
        m_popDuration = popDur; m_spawnDuration = spawnDur; m_spawnChaos = chaos;
    }
    float GetPopDuration() const { return m_popDuration; }
    float GetSpawnDuration() const { return m_spawnDuration; }
    float GetSpawnChaos() const { return m_spawnChaos; }

    void SetScalingParams(float p2u, float scale) { m_pixelToUnit = p2u; m_wingGlobalScale = scale; }
    float GetPixelToUnit() const { return m_pixelToUnit; }
    float GetWingGlobalScale() const { return m_wingGlobalScale; }

    void SetFXClickThrough(bool enable) { if (m_fxWindow) m_fxWindow->SetClickThrough(enable); }
    bool IsFXClickThrough() const { return m_fxWindow ? m_fxWindow->IsClickThrough() : true; }
    Camera* GetFXCamera() const { return m_fxCamera.get(); }

    void TriggerBouncingWindows(NaviBoss* boss);
    std::vector<Bullet*> GetProjectiles();

    void TriggerOrbitalBlaster(NaviBoss* boss);
    void TriggerTargetedBlaster(NaviBoss* boss); // Deklarasikan fungsi triggernya

    void SetAITarget(Player* p) { m_aiTarget = p; }
    void SetAIEnabled(bool val) { m_aiEnabled = val; }
    bool IsAIEnabled() const { return m_aiEnabled; }

    void TakeDamage(int damage, DirectX::XMFLOAT3 hitPos);
    void SetHP(int hp) { m_bossHP = hp; }
    int  GetHP() const { return m_bossHP; }
    int  GetMaxHP() const { return m_bossMaxHP; }
    bool IsDead() const { return m_bossHP <= 0; }

    void SetOverdriveSpriteScale(float scale) { m_overdriveSpriteScale = scale; }
    float GetOverdriveSpriteScale() const { return m_overdriveSpriteScale; }

    struct OrbitalBlaster {
        bool active = false;
        int state = 0;
        float timer = 0.0f;
        float baseX = 0.0f;
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 targetPos = { 0.0f, 0.0f, 0.0f };
        float beamScaleX = 0.0f;
        std::string windowName;
        std::string beamWindowName;
        float beamCurrentLength = 0.0f;

        Effekseer::Handle chargeEffectHandle = -1;
        Effekseer::Handle fireEffectHandle = -1; // [NEW] Handle untuk efek tembakan (LASER.efk)

        bool isTargeted = false;
    };

    // ==========================================
    // PARAMETER ORBITAL BLASTER (REFINED)
    // ==========================================
    struct BlasterParams {
        float cannonWindowSize = 200.0f;
        float cannonVisualScale = 2.0f;
        float cannonHitboxRadius = 1.5f;
        float cannonShakeIntensity = 0.4f;

        float beamVisualWidth = 2.0f;
        float beamHitboxWidth = 3.0f;
        float beamMaxLength = 120.0f;
        float beamGrowSpeed = 20.0f;
        float beamSlideSpeed = 10.0f;
        int   beamDamage = 20;

        float chargeDelay = 1.0f;
        float fireDuration = 0.8f;

        int   spawnCount = 4;
        float spawnDelay = 0.5f;
        float spawnSpreadX = 40.0f;

        // ==========================================
        // [NEW] PARAMETER VFX & TIMING
        // ==========================================
        float dropInDuration = 0.4f;    // Ganti magic number 0.4f
        float retreatSpeed = 40.0f;     // Ganti magic number 40.0f
        float windowFadeSpeed = 25.0f;  // Ganti magic number 25.0f

        std::string chargeEffectPath = "Data/Effect/TEST.efk";
        float chargeEffectScale = 1.0f;

        std::string fireEffectPath = "Data/Effect/LASER.efk";
        float fireEffectScale = 1.0f;

        float effectPitchDegrees = 90.0f;
        DirectX::XMFLOAT3 effectOffset = { 0.0f, 0.0f, 2.0f }; // Offset maju searah Z
    };
    BlasterParams& GetBlasterParams() { return m_blasterParams; }
    const std::vector<std::shared_ptr<OrbitalBlaster>>& GetBlasters() const { return m_blasters; }
    TargetedBlasterParams& GetTargetedBlasterParams() { return m_targetedBlasterParams; }

    // ==========================================
    // PARAMETER WINDOW MEMANTUL (REFINED)
    // ==========================================
    struct BouncingBulletParams {
        float speed = 35.0f;
        int   maxBounces = 5;
        int   spawnCount = 3;

        float spawnDelay = 0.2f;

        // Pemisahan Ukuran Jendela OS
        float windowWidth = 230.0f;
        float windowHeight = 230.0f;

        // Pemisahan Visual vs Hitbox
        float visualScale = 12.0f;    // Ukuran model 3D (bola)
        float hitboxRadius = 2.0f;   // Radius deteksi tabrakan fisik

        int   damage = 10;
    };

    BouncingBulletParams& GetBouncingParams() { return m_bouncingParams; }

    struct BoomerangParams {
        float speed = 40.0f;
        float windowSize = 150.0f;
        float visualScale = 5.0f;
        float hitboxRadius = 1.0f;

        int   spawnCount = 5;
        float spawnDelay = 1.0f;
        float turnSpeed = 3.0f;
        int   damage = 10;

        // =========================================================
        // [NEW] PARAMETER KONTROL JARAK & AREA SCREEN
        // =========================================================
        float maxTravelDistance = 35.0f;   // Seberapa jauh bumerang melaju sebelum ngerem
        bool  spawnBottomHalfOnly = true; // true = Hanya setengah bawah monitor, false = Seluruh layar
    };

    BoomerangParams& GetBoomerangParams() { return m_boomerangParams; }
    void TriggerBoomerang(NaviBoss* boss);


    struct UndyneSpearWindow {
        std::unique_ptr<Bullet> bullet;
        std::string windowName;

        int state = 0; // 0 = Muncul & Membidik, 1 = Telegraph Laser, 2 = Meluncur
        float timer = 0.0f;

        DirectX::XMFLOAT3 lockDir = { 0,0,1 }; // Arah tembakan yang sudah dikunci
        float currentSpeed = 0.0f;             // Untuk logika Ease-in

        bool isPreparedForDestroy = false;
    };

    void TriggerUndyneSpear(NaviBoss* boss);
    UndyneSpearParams& GetUndyneParams() { return m_undyneParams; }

    // [BARU] API Kandang (Cage) untuk dibaca oleh CollisionManager
    bool IsPlayerCaged() const { return m_isPlayerCaged; }
    DirectX::XMFLOAT3 GetCagePos() const { return m_cagePos; }
    float GetCageSize() const { return m_cageSizeWorld; }
    void DamageCage(int dmg);

private:
    void GenerateButterflyWings();
    void UpdateAI(float dt, NaviBoss* boss);
    void TriggerCageFirstHitDialogue(NaviBoss* boss);


private:
    // Komponen FX
    Beyond::Window* m_fxWindow = nullptr;
    std::shared_ptr<Camera> m_fxCamera;
    std::unique_ptr<Sprite> m_wingSprite;

    std::vector<WingNode> m_leftWingData;
    std::vector<WingNode> m_rightWingData;

    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    // State Animasi
    enum class WingState { Expanding, Idle };
    WingState m_wingState = WingState::Expanding;
    float m_wingStateTimer = 0.0f;
    const float WING_EXPAND_DURATION = 2.0f;

    unsigned int m_wingSeed = 1337;
    float m_wingFlickerTimer = 0.0f;
    float m_nextFlickerTarget = 0.2f;

    float m_popDuration = 0.15f;
    float m_spawnDuration = 1.5f;
    float m_spawnChaos = 0.5f;

    float m_glitchTimer = 0.0f;
    float m_wingXOffset = 4.2f;
    float m_wingZOffset = -0.856f;
    float m_wingFlapSpeed = 0.7f;
    float m_wingFlapIntensity = 0.02f;

    float m_pixelToUnit = 40.0f;
    float m_wingGlobalScale = 2.5f;

    //---- bullets ------
    std::vector<BouncingWindowBullet> m_bouncingBullets;
    int m_bounceCounter = 0;

    std::shared_ptr<Model> m_placeholderModel;
    OrbitalBlaster m_testBlaster; // Untuk dicoba 1 dulu

    BlasterParams m_blasterParams; // Instance parameter
    TargetedBlasterParams m_targetedBlasterParams;

    BouncingBulletParams m_bouncingParams; // Instance parameter

    std::unique_ptr<Primitive> m_solidRenderer;


    std::vector<std::shared_ptr<OrbitalBlaster>> m_blasters;
    bool  m_isSpawningBlasters = false;
    int   m_blastersSpawned = 0;
    float m_blasterSpawnTimer = 0.0f;
    bool  m_isSpawningTargetedBlasters = false;
    int   m_targetedBlastersSpawned = 0;
    float m_targetedBlasterSpawnTimer = 0.0f;

    std::vector<BoomerangWindowBullet> m_boomerangs;
    BoomerangParams m_boomerangParams;

    bool  m_isSpawningBoomerangs = false;
    int   m_boomerangsSpawned = 0;
    float m_boomerangSpawnTimer = 0.0f;

    bool  m_isSpawningBouncing = false;
    int   m_bouncingSpawned = 0;
    float m_bouncingSpawnTimer = 0.0f;


    UndyneSpearParams m_undyneParams;
    std::vector<UndyneSpearWindow> m_undyneSpears;

    bool  m_isSpawningUndynes = false;
    int   m_undynesSpawned = 0;
    float m_undyneSpawnTimer = 0.0f;

    std::vector<int> m_undyneSpawnIndices;

    Player* m_aiTarget = nullptr;
    bool    m_aiEnabled = false;
    float   m_aiGlobalCooldown = 1.0f;
    float   m_cdBouncing = 1.0f;
    float   m_cdBoomerang = 4.0f;
    float   m_cdOrbitalBlaster = 7.0f;
    float   m_cdTargetedBlaster = 10.0f;
    float   m_cdUndyne = 13.0f;

    int   m_bossMaxHP = 10000;
    int   m_bossHP = 10000;
    float m_hitFlashTimer = 0.0f;

    // [BARU] Parameter Kandang
    NaviBoss* m_bossRef = nullptr; // Untuk menyimpan pointer boss sementara
    bool m_isPlayerCaged = false;
    int m_cageMaxHP = 1000;
    int m_cageHP = 1000;             // Sesuaikan dengan damage peluru player-mu
    bool m_isCageOverdrive = false;
    DirectX::XMFLOAT3 m_cagePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_cageWindowPos = { 0.0f, 0.0f, 0.0f };
    float m_cageShakeTimer = 0.0f;
    float m_cageShakeDuration = 0.18f;
    float m_cageShakeIntensity = 0.22f;
    float m_cageSizeWorld = 7.5f;  // Hasil dari 300px / PixelToUnitRatio
    std::string m_cageWindowName = "player_cage_window";

    // [BARU] Overdrive Sprite
    std::unique_ptr<Sprite> m_overdriveSprite;
    float m_overdriveSpriteScale = 0.02f;

    float m_overdriveAlpha = 0.0f;
    float m_overdriveFadeSpeed = 2.0f;

    // ----- Opening Dialogue (tracking window, mengikuti posisi bos) -----
    std::unique_ptr<UIDialogueBox> m_dialogueBox;
    Beyond::Window* m_dialogueWindow = nullptr;
    std::shared_ptr<Camera> m_dialogueCamera;
    DirectX::XMFLOAT3      m_dialogueWorldPos = { 0.0f, 0.0f, 0.0f }; // Titik anchor di dunia
    const float            m_dialogueWindowW = 420.0f;
    const float            m_dialogueWindowH = 160.0f;
    const std::string      m_dialogueWindowName = "navi_dialogue";
    bool                   m_isDialogueActive = false;

    // ----- Overdrive Dialogue (one-shot saat player masuk overdrive) -----
    bool m_overdriveDialogueTriggered = false; // Guard agar hanya muncul sekali

    bool m_cageFirstHitTriggered = false;
};