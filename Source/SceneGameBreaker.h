#pragma once

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h> 

// Game Systems
#include "Framework.h"
#include "Scene.h"
#include "SceneGameBeyond.h"
#include "GameWindow.h"
#include "CameraController.h"
#include "UberShader.h"
#include "System/AudioManager.h"
#include "System/Sprite.h"
#include "System/Light.h"
#include "System/Collision.h"
#include "System/CollisionManager.h"
#include "BitmapFont.h"     
#include "ResourceManager.h"

// Game Objects
#include "Ball.h"
#include "BlockManager.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Paddle.h"
#include "Player.h"
#include "Stage.h"

#include "CinematicDirector.h" // Logic Kamera ada di sini
#include "GUISceneGameBreaker.h" // Logic GUI ada di sini
#include "PostProcessManager.h"

#include "UIImpactDisplay.h"

class SceneGameBreaker : public Scene
{
    // Memberi akses ke class GUI untuk edit variabel private
    friend class GameBreakerGUI;

public:
    SceneGameBreaker();
    ~SceneGameBreaker() override;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return mainCamera.get(); }

private:
    // =========================================================
    // CONFIGURATION CONSTANTS
    // =========================================================
    struct Config {
        // Camera
        static constexpr float CAM_FOV = 45.0f;
        static constexpr float CAM_NEAR = 0.1f;
        static constexpr float CAM_FAR = 1000.0f;
        static constexpr float CAM_START_HEIGHT = 20.0f; 

        // Gameplay
        static constexpr int   TRIGGER_BLOCK_COUNT = 40;
        static constexpr float BALL_SPAWN_Z_OFFSET = 0.38f;

        // Screen Shake (On Block Hit)
        static constexpr float SHAKE_DURATION = 1.3f;
        static constexpr float SHAKE_AMP_POS = 0.3f;
        static constexpr float SHAKE_AMP_ROT = 0.3f;
        static constexpr float SHAKE_FREQ = 35.0f;
        static constexpr float SHAKE_TRAUMA = 4.0f;

        // Post Process / CRT Effect
        static constexpr float FX_CRT_BASE_STRENGTH = 0.2f;      
        static constexpr float FX_CRT_ROTATION_TARGET = 0.45f;   
        static constexpr float FX_TRANSITION_WINDOW = 0.2f;      
        static constexpr float FX_GLITCH_FACTOR = 0.7f;          
    };

    // =========================================================
    // INTERNAL HELPER FUNCTIONS
    // =========================================================
    void RenderScene(float elapsedTime, Camera* camera);
    void UpdateGameTriggers(float elapsedTime);

    DirectX::XMFLOAT3 GetMouseOnGround(Camera* camera);

    // =========================================================
    // SPRITE CONTROLS & LAYOUT
    // =========================================================
    struct UI_LayoutData {
        char name[32];
        float x, y;
        float lineSpacing;
        float scale;
        float color[4];
    };

    DirectX::XMFLOAT3 m_spritePos = { 0.143f, 13.0f, 0.0f };
    DirectX::XMFLOAT2 m_spriteSize = { 16.0f, 9.0f };
    float m_spriteScale = 0.645f;
    float m_spritePitch = 90.0f;
    float m_spriteYaw = 0.0f;
    float m_spriteRoll = 0.0f;

    std::string tutorialText = "[A/D] Move [SPACE] Shoot";
    UI_LayoutData tutorialLayout = {
         "Tutorial Panel", 349.0f, 836.0f, 0.0f, 0.625f, { 0.96f, 0.80f, 0.23f, 1.0f }
    };

    // =========================================================
    // RUNTIME STATE
    // =========================================================
    struct PostProcessState {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableChromatic = true;
        bool EnableCRT = true;
    };

    Ball* ball = nullptr;
    Paddle* paddle = nullptr;
    Player* player = nullptr;
    std::unique_ptr<BlockManager> blockManager;
    std::unique_ptr<CollisionManager> m_collisionManager;
    std::unique_ptr<EnemyManager> m_enemyManager;
    std::unique_ptr<ItemManager> m_itemManager;
    std::unique_ptr<Stage> m_stage;
    std::shared_ptr<Camera> mainCamera;

    // =========================================================
    // RUNTIME STATE
    // =========================================================
    struct CheckpointData
    {
        bool isValid = false;
        DirectX::XMFLOAT3 position;
        int savedBlockCount = 0;
        int savedGameStage = 0;
        bool savedCanShield = false;
        bool savedCanShoot = false;
    };
    CheckpointData m_checkpoint;

    void SaveCheckpoint(const DirectX::XMFLOAT3& checkpointPos);
    void LoadCheckpoint();

    // --- DEATH ANIMATION STATE ---
    bool m_isDying = false;       
    float m_deathTimer = 0.0f;    

    const float DEATH_DELAY_DURATION = 0.5f; 
    const float DEATH_FADE_DURATION = 3.0f;  

    // Helper function
    void StartPlayerDeathSequence();

    // Transition State
    bool m_isTransitioning = false;
    float m_transitionTimer = 0.0f;
    const float TRANSITION_DURATION = 10.0f; 

    void StartLevelTransition();

    // Camera Settings
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 cameraTarget = { 0.0f, 0.0f, 0.0f };

    // Light
    LightManager m_lightManager;

    // Graphics
    std::unique_ptr<Sprite> m_spriteBorderBreaker;
    std::unique_ptr<Sprite> m_spriteDEBUG_LAYOUT; // Jika masih dipakai
    std::unique_ptr<Sprite> m_spriteSubText;
    std::unique_ptr<Sprite> m_spriteSubTextMouseShift;
    std::unique_ptr<Sprite> m_spriteSubTextMouseSpace;
    std::unique_ptr<PostProcessManager> m_postProcess;
    UberShader::UberData uberParams;
    PostProcessState m_fxState;

    // Visual Config
    DirectX::XMFLOAT4 bgSpriteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const char* pathBorderBreaker = "Data/Sprite/Scene Breaker/Sprite_BorderBreakertransparent.png";

    // State Variables
    bool m_modelIntroFinished = false; 
    float m_modelIntroTimer = 0.0f;    
    float m_modelIntroSpeed = 0.003f;  
    bool m_hasTriggered = false;
    float m_globalTime = 0.0f;
    float m_bgRotation = 0.0f;
    bool m_isShakeEnabled = false;
    float m_configFineDensity = 30.0f;
    float m_configZoomDensity = 0.0f;
    bool m_introFinished = false;
    bool m_hasTriggeredDefenseTutorial = false;
    bool m_showDefenseSubtext = false;
    bool m_defenseConditionMet = false;
    float m_defenseStopTimer = 0.0f;
    bool m_hasTriggeredAttackTutorial = false;
    bool m_showAttackSubtext = false;
    bool m_attackConditionMet = false;
    float m_attackStopTimer = 0.0f;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    // [PENTING] Director yang mengatur kamera sekarang
    std::unique_ptr<CinematicDirector> m_director;

    std::unique_ptr<UIImpactDisplay> m_impactDisplay;

    bool m_hasTriggeredEscapeSequence = false;
    bool m_showSubText = false; // Status apakah subtext muncul
    int m_mashLoopCount = 0;    // Hitungan loop mash

    float m_mashTimer = 0.0f;       // Penghitung waktu manual
    bool m_isMashFirstRun = true;   // Penanda apakah ini trigger pertama (1 detik)
    bool m_mashSequenceActive = false;

    float m_respawnTimer = 0.0f;
    const float RESPAWN_FADE_DURATION = 3.0f;
};