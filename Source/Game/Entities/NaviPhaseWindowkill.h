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

// --- Wing Structure: Defines properties for individual butterfly wings ---
struct WingNode {
    DirectX::XMFLOAT2 localOffset; // Offset relative to parent
    DirectX::XMFLOAT2 targetOffset; // Target destination offset for animation
    DirectX::XMFLOAT2 size;          // Dimensions of the wing
    float flapOffset;               // Phase offset for flapping animation
    float spawnDelay = 0.0f;        // Delay before wing appears
    float animScale = 0.0f;         // Current animation scaling
    bool isClosing = false;         // State flag for wing closing animation
};

// --- Bouncing Bullet Structure: Tracks bullets that bounce off window edges ---
struct BouncingWindowBullet {
    std::unique_ptr<Bullet> bullet;
    std::string windowName;         // The window boundary identifier
    int bounceCount = 0;            // Current number of bounces
    int maxBounces = 8;             // Maximum allowed bounces
};

// --- Boomerang Bullet Structure: Handles projectile logic for returning bullets ---
struct BoomerangWindowBullet {
    std::unique_ptr<Bullet> bullet;
    std::string windowName;
    int state = 0;                  // 0 = Entering, 1 = Returning
    int spawnSide = 1;              // 1 = Right, -1 = Left
    float startX = 0.0f;            // Initial X coordinate
    float targetX = 0.0f;           // Turning point X
    float targetVelX = 0.0f;        // Target velocity for return path
};

// =========================================================
// UNDYNE SPEAR STRUCT: Configuration for spear wave pattern
// =========================================================
struct UndyneSpearParams {
    int   count = 10;                // Number of spears to spawn
    float spawnDelay = 0.4f;         // Delay between each spear spawn
    float hoverDuration = 1.0f;      // Duration before launching
    float telegraphDuration = 1.0f;  // Duration of laser warning
    float startSpeed = 10.0f;        // Initial projectile speed
    float maxSpeed = 60.0f;          // Maximum velocity
    float acceleration = 10.0f;      // Speed increase rate
    int   damage = 10;               // Damage value
    float arcRadius = 20.0f;         // Curvature radius
    float arcCenterX = 0.0f;         // Center X for arc
    float arcCenterZ = -15.0f;       // Center Z for arc
    float arcMinAngle = 20.0f;       // Minimum angle of arc
    float arcMaxAngle = 160.0f;      // Maximum angle of arc
};

// =========================================================
// TARGETED BLASTER STRUCT: Config for player-tracking blasters
// =========================================================
struct TargetedBlasterParams {
    int   spawnCount = 8;
    float spawnDelay = 0.8f;         // Delay between blasts
    float dropInDuration = 0.2f;     // Speed to enter screen
    float chargeDelay = 0.2f;        // Warning time before firing
    float fireDuration = 1.0f;       // How long the laser stays active
    float fixedTargetZ = 10.0f;      // Constant Z depth
    float beamHitboxWidth = 3.0f;    // Width of collision box
    int   beamDamage = 20;           // Damage on hit
};

class NaviPhaseWindowkill : public INaviPhase {
public:
    NaviPhaseWindowkill(Player* player = nullptr);
    ~NaviPhaseWindowkill() override = default;

    // --- Life Cycle Methods (INaviPhase interface) ---
    void Enter(NaviBoss* boss) override;
    void Update(float dt, NaviBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* currentCamera, NaviBoss* boss) override;
    void Exit(NaviBoss* boss) override;

    // --- Wing Animation Control ---
    void ReplayAnimation(); // Resets butterfly wing animations

    // Set/Get for wing flap behavior
    void SetWingFlapParams(float speed, float intensity) { m_wingFlapSpeed = speed; m_wingFlapIntensity = intensity; }
    float GetWingFlapSpeed() const { return m_wingFlapSpeed; }
    float GetWingFlapIntensity() const { return m_wingFlapIntensity; }

    // Set/Get for wing position offsets
    void SetWingOffsets(float xOffset, float zOffset) { m_wingXOffset = xOffset; m_wingZOffset = zOffset; }
    float GetWingOffsetX() const { return m_wingXOffset; }
    float GetWingOffsetZ() const { return m_wingZOffset; }

    // Set/Get for randomized wing seed
    void SetWingSeed(unsigned int seed) {
        if (m_wingSeed != seed) { m_wingSeed = seed; GenerateButterflyWings(); }
    }
    unsigned int GetWingSeed() const { return m_wingSeed; }

    // Set/Get for spawn timing and randomness
    void SetSpawnParams(float popDur, float spawnDur, float chaos) {
        m_popDuration = popDur; m_spawnDuration = spawnDur; m_spawnChaos = chaos;
    }
    float GetPopDuration() const { return m_popDuration; }
    float GetSpawnDuration() const { return m_spawnDuration; }
    float GetSpawnChaos() const { return m_spawnChaos; }

    // Set/Get for scale factors
    void SetScalingParams(float p2u, float scale) { m_pixelToUnit = p2u; m_wingGlobalScale = scale; }
    float GetPixelToUnit() const { return m_pixelToUnit; }
    float GetWingGlobalScale() const { return m_wingGlobalScale; }

    // FX Control
    void SetFXClickThrough(bool enable) { if (m_fxWindow) m_fxWindow->SetClickThrough(enable); }
    bool IsFXClickThrough() const { return m_fxWindow ? m_fxWindow->IsClickThrough() : true; }
    Camera* GetFXCamera() const { return m_fxCamera.get(); }

    // Bullet Triggers
    void TriggerBouncingWindows(NaviBoss* boss);
    std::vector<Bullet*> GetProjectiles();
    void TriggerOrbitalBlaster(NaviBoss* boss);
    void TriggerTargetedBlaster(NaviBoss* boss);

    // AI/Targeting
    void SetAITarget(Player* p) { m_aiTarget = p; }
    void SetAIEnabled(bool val) { m_aiEnabled = val; }
    bool IsAIEnabled() const { return m_aiEnabled; }

    // Boss State Management
    void TakeDamage(int damage, DirectX::XMFLOAT3 hitPos);
    void SetHP(int hp) { m_bossHP = hp; }
    int  GetHP() const { return m_bossHP; }
    int  GetMaxHP() const { return m_bossMaxHP; }
    bool IsDead() const { return m_bossHP <= 0; }

    // Visuals/Misc
    void SetOverdriveSpriteScale(float scale) { m_overdriveSpriteScale = scale; }
    float GetOverdriveSpriteScale() const { return m_overdriveSpriteScale; }
    bool IsReadyToChangeScene() const { return m_isDying && m_deathTimer >= 7.0f; }

    // --- Orbital Blaster Definition ---
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
        Effekseer::Handle fireEffectHandle = -1;

        bool isTargeted = false;
    };

    // --- Orbital Blaster Settings ---
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
        float dropInDuration = 0.4f;
        float retreatSpeed = 40.0f;
        float windowFadeSpeed = 25.0f;
        std::string chargeEffectPath = "Data/Effect/TEST.efk";
        float chargeEffectScale = 1.0f;
        std::string fireEffectPath = "Data/Effect/LASER.efk";
        float fireEffectScale = 1.0f;
        float effectPitchDegrees = 90.0f;
        DirectX::XMFLOAT3 effectOffset = { 0.0f, 0.0f, 2.0f };
    };
    BlasterParams& GetBlasterParams() { return m_blasterParams; }
    const std::vector<std::shared_ptr<OrbitalBlaster>>& GetBlasters() const { return m_blasters; }
    TargetedBlasterParams& GetTargetedBlasterParams() { return m_targetedBlasterParams; }

    // --- Bouncing Bullet Settings ---
    struct BouncingBulletParams {
        float speed = 35.0f;
        int   maxBounces = 5;
        int   spawnCount = 3;
        float spawnDelay = 0.2f;
        float windowWidth = 230.0f;
        float windowHeight = 230.0f;
        float visualScale = 12.0f;
        float hitboxRadius = 2.0f;
        int   damage = 10;
    };
    BouncingBulletParams& GetBouncingParams() { return m_bouncingParams; }

    // --- Boomerang Settings ---
    struct BoomerangParams {
        float speed = 40.0f;
        float windowSize = 150.0f;
        float visualScale = 5.0f;
        float hitboxRadius = 1.0f;
        int   spawnCount = 5;
        float spawnDelay = 1.0f;
        float turnSpeed = 3.0f;
        int   damage = 10;
        float maxTravelDistance = 35.0f;
        bool  spawnBottomHalfOnly = true;
    };
    BoomerangParams& GetBoomerangParams() { return m_boomerangParams; }
    void TriggerBoomerang(NaviBoss* boss);

    // --- Undyne Spear Wrapper ---
    struct UndyneSpearWindow {
        std::unique_ptr<Bullet> bullet;
        std::string windowName;
        int state = 0;
        float timer = 0.0f;
        DirectX::XMFLOAT3 lockDir = { 0,0,1 };
        float currentSpeed = 0.0f;
        bool isPreparedForDestroy = false;
    };
    void TriggerUndyneSpear(NaviBoss* boss);
    UndyneSpearParams& GetUndyneParams() { return m_undyneParams; }

    // --- Player Cage Accessors ---
    bool IsPlayerCaged() const { return m_isPlayerCaged; }
    DirectX::XMFLOAT3 GetCagePos() const { return m_cagePos; }
    float GetCageSize() const { return m_cageSizeWorld; }
    void DamageCage(int dmg);

private:
    void GenerateButterflyWings();
    void UpdateAI(float dt, NaviBoss* boss);
    void TriggerCageFirstHitDialogue(NaviBoss* boss);

private:
    // --- FX and Wing Components ---
    Beyond::Window* m_fxWindow = nullptr;
    std::shared_ptr<Camera> m_fxCamera;
    std::unique_ptr<Sprite> m_wingSprite;
    std::vector<WingNode> m_leftWingData;
    std::vector<WingNode> m_rightWingData;

    // --- Screen/Display Dimensions ---
    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    // --- Animation State Machine ---
    enum class WingState { Expanding, Idle };
    WingState m_wingState = WingState::Expanding;
    float m_wingStateTimer = 0.0f;
    const float WING_EXPAND_DURATION = 2.0f;

    // --- Visual Randomization and Timing ---
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

    // --- Bullet Storage and Management ---
    std::vector<BouncingWindowBullet> m_bouncingBullets;
    int m_bounceCounter = 0;
    std::shared_ptr<Model> m_placeholderModel;
    OrbitalBlaster m_testBlaster;

    // --- Parameter Instances ---
    BlasterParams m_blasterParams;
    TargetedBlasterParams m_targetedBlasterParams;
    BouncingBulletParams m_bouncingParams;
    std::unique_ptr<Primitive> m_solidRenderer;

    // --- Spawning Control (Blasters) ---
    std::vector<std::shared_ptr<OrbitalBlaster>> m_blasters;
    bool  m_isSpawningBlasters = false;
    int   m_blastersSpawned = 0;
    float m_blasterSpawnTimer = 0.0f;
    bool  m_isSpawningTargetedBlasters = false;
    int   m_targetedBlastersSpawned = 0;
    float m_targetedBlasterSpawnTimer = 0.0f;

    // --- Spawning Control (Boomerangs) ---
    std::vector<BoomerangWindowBullet> m_boomerangs;
    BoomerangParams m_boomerangParams;
    bool  m_isSpawningBoomerangs = false;
    int   m_boomerangsSpawned = 0;
    float m_boomerangSpawnTimer = 0.0f;

    // --- Spawning Control (Bouncing) ---
    bool  m_isSpawningBouncing = false;
    int   m_bouncingSpawned = 0;
    float m_bouncingSpawnTimer = 0.0f;

    // --- Spawning Control (Spears) ---
    UndyneSpearParams m_undyneParams;
    std::vector<UndyneSpearWindow> m_undyneSpears;
    bool  m_isSpawningUndynes = false;
    int   m_undynesSpawned = 0;
    float m_undyneSpawnTimer = 0.0f;
    std::vector<int> m_undyneSpawnIndices;

    // --- AI/Game Logic Control ---
    Player* m_aiTarget = nullptr;
    bool    m_aiEnabled = false;
    float   m_aiGlobalCooldown = 1.0f;
    float   m_cdBouncing = 1.0f;
    float   m_cdBoomerang = 4.0f;
    float   m_cdOrbitalBlaster = 7.0f;
    float   m_cdTargetedBlaster = 10.0f;
    float   m_cdUndyne = 13.0f;

    // --- Boss Health/Hit Logic ---
    int   m_bossMaxHP = 10000;
    int   m_bossHP = 10000;
    float m_hitFlashTimer = 0.0f;

    // --- Cage Mechanics ---
    NaviBoss* m_bossRef = nullptr;
    bool m_isPlayerCaged = false;
    int m_cageMaxHP = 1000;
    int m_cageHP = 1000;
    bool m_isCageOverdrive = false;
    DirectX::XMFLOAT3 m_cagePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_cageWindowPos = { 0.0f, 0.0f, 0.0f };
    float m_cageShakeTimer = 0.0f;
    float m_cageShakeDuration = 0.18f;
    float m_cageShakeIntensity = 0.22f;
    float m_cageSizeWorld = 7.5f;
    std::string m_cageWindowName = "player_cage_window";

    // --- Overdrive Logic ---
    std::unique_ptr<Sprite> m_overdriveSprite;
    float m_overdriveSpriteScale = 0.02f;
    float m_overdriveAlpha = 0.0f;
    float m_overdriveFadeSpeed = 2.0f;

    // --- Dialogue Handling ---
    std::unique_ptr<UIDialogueBox> m_dialogueBox;
    Beyond::Window* m_dialogueWindow = nullptr;
    std::shared_ptr<Camera> m_dialogueCamera;
    DirectX::XMFLOAT3      m_dialogueWorldPos = { 0.0f, 0.0f, 0.0f };
    const float            m_dialogueWindowW = 420.0f;
    const float            m_dialogueWindowH = 160.0f;
    const std::string      m_dialogueWindowName = "navi_dialogue";
    bool                   m_isDialogueActive = false;
    bool                   m_overdriveDialogueTriggered = false;
    bool                   m_cageFirstHitTriggered = false;

    // --- Death/Clean-up Sequence ---
    bool  m_isDying = false;
    float m_deathTimer = 0.0f;
    Effekseer::Handle m_deathVfxHandle = -1;
    bool m_deathCleanupDone = false;
    bool m_deathWindowRaised = false;
};