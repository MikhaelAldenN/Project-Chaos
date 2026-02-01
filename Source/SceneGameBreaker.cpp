#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include "Framework.h"
#include "SceneGameBreaker.h"
#include "WindowManager.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "JuiceEngine.h"

using namespace DirectX;

SceneGameBreaker::SceneGameBreaker()
{
    float screenW = 1920;
    float screenH = 1080;
    if (auto window = Framework::Instance()->GetMainWindow())
    {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // 1. Setup Main Camera
    mainCamera = std::make_shared<Camera>();
    mainCamera->SetPerspectiveFov(XMConvertToRadians(Config::CAM_FOV), screenW / screenH, Config::CAM_NEAR, Config::CAM_FAR);

    auto& camCtrl = CameraController::Instance();
    camCtrl.SetActiveCamera(mainCamera);

    XMFLOAT3 startPos = cameraPosition;
    startPos.x = 0.0f; startPos.z = 0.0f; startPos.y = Config::CAM_START_HEIGHT;
    mainCamera->SetPosition(startPos);
    mainCamera->LookAt(cameraTarget);

    camCtrl.SetControlMode(CameraControlMode::FixedStatic);
    camCtrl.SetFixedSetting(startPos);
    camCtrl.SetTarget(cameraTarget);

    // 2. Initialize Assets
    m_spriteBorderBreaker = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathBorderBreaker);
    m_stage = std::make_unique<Stage>(Graphics::Instance().GetDevice());

    ball = new Ball();
    paddle = new Paddle();
    paddle->SetAIEnabled(false);
    player = new Player();
    player->SetMoveSpeed(Player::MovementSettings::BreakerSpeed);
    player->SetInputEnabled(false);
    player->GetMovement()->SetRotationY(DirectX::XM_PI);
    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player);

    m_enemyManager = std::make_unique<EnemyManager>();
    m_enemyManager->Initialize(Graphics::Instance().GetDevice());

    m_itemManager = std::make_unique<ItemManager>();
    m_itemManager->Initialize(Graphics::Instance().GetDevice());

    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(player, m_stage.get(), blockManager.get(), m_enemyManager.get(), m_itemManager.get());

    m_collisionManager->SetOnPlayerDeathCallback([this]() { StartPlayerDeathSequence(); });
    m_collisionManager->SetOnCheckpointReachCallback([this](DirectX::XMFLOAT3 pos){ SaveCheckpoint(pos); });
    // Callback untuk Shake saat blok hancur
    blockManager->SetOnBlockHitCallback([this]()
        {
            if (m_isShakeEnabled)
            {
                ShakeSettings hitShake;
                hitShake.Duration = Config::SHAKE_DURATION;
                hitShake.AmplitudePos = Config::SHAKE_AMP_POS;
                hitShake.AmplitudeRot = Config::SHAKE_AMP_ROT;
                hitShake.Frequency = Config::SHAKE_FREQ;
                hitShake.TraumaFalloff = Config::SHAKE_TRAUMA;
                JuiceEngine::Instance().TriggerShake(hitShake);
            }
        });

    // 3. Setup Director & Shader
    m_director = std::make_unique<CinematicDirector>();

    m_postProcess = std::make_unique<PostProcessManager>();
    m_postProcess->Initialize((int)screenW, (int)screenH);

    // Initialize Impact Display
    m_impactDisplay = std::make_unique<UIImpactDisplay>();
    m_impactDisplay->Initialize(Graphics::Instance().GetDevice());

    // Sesuaikan ukuran layar awal
    m_impactDisplay->OnResize(screenW, screenH);

    m_spriteSubText = std::make_unique<Sprite>(Graphics::Instance().GetDevice(),
        "Data/Sprite/Scene Breaker/Sprite_SubText_SPACEKEYRENDA.png");
    m_spriteSubTextMouseShift = std::make_unique<Sprite>(Graphics::Instance().GetDevice(),
        "Data/Sprite/Scene Breaker/Sprite_SubText_MOUSESHIFT.png");
    m_spriteSubTextMouseSpace = std::make_unique<Sprite>(Graphics::Instance().GetDevice(),
        "Data/Sprite/Scene Breaker/Sprite_SubText_MOUSESPACE.png");

    m_lastMouseX = Input::Instance().GetMouse().GetPositionX();
    m_lastMouseY = Input::Instance().GetMouse().GetPositionY();
}

SceneGameBreaker::~SceneGameBreaker()
{
    CameraController::Instance().ClearCamera();
    if (player) delete player;
    if (paddle) delete paddle;
    if (ball) delete ball;
}

void SceneGameBreaker::Update(float elapsedTime)
{
    m_globalTime += elapsedTime;
    if (m_globalTime > 1000.0f) m_globalTime -= 1000.0f;

    if (!m_modelIntroFinished) { m_modelIntroTimer += elapsedTime; }

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // Update Entities
    if (paddle)
    {
        if (ball) paddle->UpdateAI(elapsedTime, ball, blockManager.get());
        paddle->Update(elapsedTime, activeCam);
    }

    if (player)
    {
        player->Update(elapsedTime, activeCam);

        float energy = player->GetShakeEnergy();
        float thresholdForm = player->breakoutSettings.thresholdFormation;
        float thresholdDest = player->breakoutSettings.thresholdDestruction;

        // =========================================================
        // UPDATE LOGIC KAMERA (VIA DIRECTOR)
        // =========================================================
        m_director->Update(elapsedTime, energy, thresholdForm, thresholdDest, player->GetMovement()->GetPosition());

        // =========================================================
        // UPDATE LOGIC GAMEPLAY 
        // =========================================================
        if (energy >= thresholdForm && blockManager && !blockManager->IsFormationActive())
        {
            blockManager->ActivateFormationMode();
            player->SetGameStage(1);
        }

        if (energy >= thresholdDest && player->GetGameStage() < 2)
        {
            if (paddle && paddle->IsActive()) paddle->SetActive(false);
            if (ball) ball->SetBoundariesEnabled(false);
            player->SetGameStage(2);
        }

        if (player->GetGameStage() == 2 && !player->IsEscaping())
        {
            if (!CameraController::Instance().IsSequencing())
            {
                player->TriggerEscape();
                CameraController::Instance().SetControlMode(CameraControlMode::FixedFollow);
            
                if (!m_hasTriggeredEscapeSequence)
                {
                    m_hasTriggeredEscapeSequence = true;
                    std::vector<ImpactEvent> seq = {
                        { ImpactType::Super,  1.2f }, // ESCAPE
                        { ImpactType::Nigero, 1.5f }  // NIGERO
                    };
                    m_impactDisplay->ShowSequence(seq);
                }
            }
        }

        if (player && player->CanUseShield() && !m_hasTriggeredDefenseTutorial)
        {
            m_hasTriggeredDefenseTutorial = true;

            if (m_impactDisplay) m_impactDisplay->Show(ImpactType::Bougyo, 2.5f);

            m_showDefenseSubtext = true;
            m_defenseConditionMet = false;
            m_defenseStopTimer = 0.0f;
        }

        if (m_showDefenseSubtext && !m_defenseConditionMet)
        {
            bool isShiftHeld = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

            int currMx = Input::Instance().GetMouse().GetPositionX();
            int currMy = Input::Instance().GetMouse().GetPositionY();
            bool isMouseMoving = (currMx != m_lastMouseX) || (currMy != m_lastMouseY);

            m_lastMouseX = currMx;
            m_lastMouseY = currMy;

            if (isShiftHeld && isMouseMoving)
            {
                m_defenseConditionMet = true;
            }
        }

        if (m_defenseConditionMet)
        {
            m_defenseStopTimer += elapsedTime;
            if (m_defenseStopTimer >= 3.5f)
            {
                m_showDefenseSubtext = false;
            }
        }

        if (player && player->CanShoot() && !m_hasTriggeredAttackTutorial)
        {
            m_hasTriggeredAttackTutorial = true;

            if (m_impactDisplay) m_impactDisplay->Show(ImpactType::Kougeki, 2.5f);

            m_showAttackSubtext = true;
            m_attackConditionMet = false;
            m_attackStopTimer = 0.0f;
        }

        if (m_showAttackSubtext && !m_attackConditionMet)
        {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000)
            {
                m_attackConditionMet = true;
            }
        }

        if (m_attackConditionMet)
        {
            m_attackStopTimer += elapsedTime;
            if (m_attackStopTimer >= 3.5f)
            {
                m_showAttackSubtext = false;
            }
        }

        if (player->IsEscaping() || player->GetGameStage() == 3)
        {
            DirectX::XMFLOAT3 pPos = player->GetPosition();
            CameraController::Instance().SetTarget(pPos);
        }
    }


    if (blockManager && player)
    {
        bool canUseShield = player->CanUseShield();
        bool canShoot = player->CanShoot();

        bool isShielding = (GetKeyState(VK_LSHIFT) & 0x8000) && canUseShield;
        bool isShooting = (GetKeyState(VK_SPACE) & 0x8000) && canShoot;

        DirectX::XMFLOAT3 mousePos = GetMouseOnGround(activeCam);
        DirectX::XMFLOAT3 playerPos = player->GetPosition();

        blockManager->UpdateShieldLogic(isShielding, mousePos, playerPos, elapsedTime);
        blockManager->UpdateShootLogic(isShooting, mousePos, playerPos, elapsedTime);
        blockManager->Update(elapsedTime, activeCam, player);

        if (ball && ball->IsActive())
        {
            blockManager->CheckCollision(ball);
        }
    }

    if (m_enemyManager)
    {
        XMFLOAT3 targetPos = { 0,0,0 };
        if (player) { targetPos = player->GetPosition(); }
        bool canAttack = (player && player->GetGameStage() == 3);
        m_enemyManager->Update(elapsedTime, activeCam, targetPos, canAttack);
    }

    if (m_itemManager) m_itemManager->Update(elapsedTime, activeCam);

    if (m_collisionManager)
    {
        m_collisionManager->Update(elapsedTime);
    }

    if (ball)
    {
        if (paddle && !ball->IsActive())
        {
            XMFLOAT3 padPos = paddle->GetPosition();
            padPos.z += Config::BALL_SPAWN_Z_OFFSET;
            padPos.y = 0.0f;
            ball->GetMovement()->SetPosition(padPos);
        }
        ball->Update(elapsedTime, activeCam);
        if (paddle && ball->IsActive()) paddle->CheckCollision(ball);
        if (player && player->GetGameStage() >= 2)
        {
            DirectX::XMFLOAT3 bPos = ball->GetMovement()->GetPosition();
            DirectX::XMFLOAT3 pPos = player->GetPosition();

            float dx = pPos.x - bPos.x;
            float dz = pPos.z - bPos.z;
            float distSq = dx * dx + dz * dz;

            if (distSq > (55.0f * 55.0f))
            {
                delete ball;     
                ball = nullptr;  
            }
        }
    }
    // Update Systems
    CameraController::Instance().Update(elapsedTime);
    JuiceEngine::Instance().Update(elapsedTime);
    UpdateGameTriggers(elapsedTime);

    // =========================================================
    // LOGIKA SCANLINE ZOOM EFFECT (Dynamic FineDensity)
    // =========================================================

    auto seqInfo = CameraController::Instance().GetSequenceProgress();

    if (m_hasTriggered && !seqInfo.IsPlaying && player)
    {
        static bool s_hasEnabledMashing = false;
        if (!s_hasEnabledMashing)
        {
            s_hasEnabledMashing = true;
            player->SetBreakoutMode(true); 
        }
    }

    // LOCK STATE
    if (seqInfo.IsPlaying && seqInfo.CurrentIndex >= 1)
    {
        m_isShakeEnabled = true;
        tutorialText = "";
    }

    float normalDensity = m_configFineDensity;
    float zoomDensity = m_configZoomDensity;
    float baseStrength = Config::FX_CRT_BASE_STRENGTH;

    // Config Rotasi
    float startRotation = 0.0f;
    float targetRotation = Config::FX_CRT_ROTATION_TARGET;

    bool isInsideProgram = m_isShakeEnabled;

    if (isInsideProgram)
    {
        // DALAM PROGRAM: Hilang
        uberParams.chromaticAberration = 0.00404f;
        uberParams.distortion = 0.0f;   
        uberParams.fineOpacity = 0.0f;
        uberParams.fineRotation = targetRotation;
        uberParams.intensity = 0.7f;
        uberParams.scanlineStrength = 0.0f;
    }
    else if (seqInfo.IsPlaying && seqInfo.CurrentIndex == 0)
    {
        // TRANSISI: Animasi dari Config A ke Config B
        float t = 0.0f;
        if (seqInfo.TotalDuration > 0.0f) t = seqInfo.CurrentTime / seqInfo.TotalDuration;
        t = std::clamp(t, 0.0f, 1.0f);

        float easedT = CameraController::ApplyEasing(t, seqInfo.CurrentEasing);

        // Interpolasi menggunakan nilai Config
        uberParams.fineDensity = normalDensity + (zoomDensity - normalDensity) * easedT;
        uberParams.fineRotation = startRotation + (targetRotation - startRotation) * easedT;

        uberParams.fineOpacity = 1.0f;
        uberParams.scanlineStrength = baseStrength;
    }
    else
    {
        // IDLE: Pakai nilai Config Normal
        uberParams.fineOpacity = 1.0f;
        uberParams.fineDensity = normalDensity;
        uberParams.fineRotation = startRotation;
        uberParams.scanlineStrength = baseStrength;
    }

    // Auto Glitch Transition
    static bool wasPlayingSequence = false;
    if (seqInfo.IsPlaying && m_fxState.MasterEnabled && m_fxState.EnableLens)
    {
        wasPlayingSequence = true;
        float t = seqInfo.CurrentTime;
        float d = seqInfo.TotalDuration;
        float remaining = d - t;
        float transitionWindow = Config::FX_TRANSITION_WINDOW;
        float glitchFactor = Config::FX_GLITCH_FACTOR;
        bool isLastShot = (seqInfo.CurrentIndex == seqInfo.TotalShots - 1);

        if (remaining <= transitionWindow && d > 0.0f && !isLastShot) {
            uberParams.glitchStrength = 1.0f - (remaining / transitionWindow);
        }
        else if (t <= transitionWindow && seqInfo.CurrentIndex > 0) {
            uberParams.glitchStrength = glitchFactor * (1.0f - (t / transitionWindow));
        }
        else {
            uberParams.glitchStrength = 0.0f;
        }
    }
    else if (wasPlayingSequence)
    {
        if (wasPlayingSequence)
        {
            if (m_fxState.MasterEnabled && m_fxState.EnableLens)
            {
                uberParams.glitchStrength = 0.0f;
            }
            wasPlayingSequence = false;
            m_introFinished = true;
        }

        if (m_introFinished && m_fxState.MasterEnabled && m_fxState.EnableLens)
        {
            uberParams.glitchStrength = 0.0f;
        }
    }
    // --- LOGIKA FISHEYE IMPACT ---
    // Cek apakah ada impact text yang sedang aktif
    if (m_impactDisplay && m_impactDisplay->IsActive())
    {
        // Ambil nilai "tendangan" (1.0 turun ke 0.0 dalam 0.2 detik)
        float kick = m_impactDisplay->GetDistortionKick();

        // [REVISI] Ubah 5.0f menjadi 0.025f
        if (kick > 0.001f)
        {
            // Hasilnya: Mulai dari 0.025 lalu turun cepat ke 0
            uberParams.distortion = kick * 0.025f;

            // Opsional: Chromatic aberration juga bisa disesuaikan kalau mau
            // uberParams.chromaticAberration = kick * 0.005f; 
        }
    }

    if (player)
    {
        float currentY = player->GetPosition().y;

        // Configuration
        const float startFadeY = -30.0f;
        const float killPlaneY = -88.0f;
        // Default Shader Values (From UberShader.h)
        const float baseSmoothness = 0.2f;
        const float baseIntensity = 0.38f;
        // Target Values (Pitch Black)
        const float targetSmoothness = 4.0f;
        const float targetIntensity = 5.0f;

        // --- DEATH SEQUENCE LOGIC ---
        if (m_isDying)
        {
            m_deathTimer += elapsedTime;

            // Phase 1: Wait 0.5s (Player is invisible)
            if (m_deathTimer < DEATH_DELAY_DURATION)
            {
                uberParams.smoothness = baseSmoothness;
                uberParams.intensity = baseIntensity;
            }
            // Phase 2: Fade Out over 3.0s
            else
            {
                float fadeTime = m_deathTimer - DEATH_DELAY_DURATION;
                float t = std::clamp(fadeTime / DEATH_FADE_DURATION, 0.0f, 1.0f);

                // Fade Vignette to Black
                uberParams.smoothness = baseSmoothness + (targetSmoothness - baseSmoothness) * t;
                uberParams.intensity = baseIntensity + (targetIntensity - baseIntensity) * t;

                // Phase 3: Finish -> Respawn
                if (t >= 1.0f)
                {
                    LoadCheckpoint();
                    // Keep screen black for one frame to match respawn start
                    uberParams.smoothness = targetSmoothness;
                    uberParams.intensity = targetIntensity;
                }
            }
        }

        // --- RESPAWN FADE IN ---
        else if (m_respawnTimer > 0.0f)
        {
            m_respawnTimer -= elapsedTime;

            float linearT = std::clamp(m_respawnTimer / RESPAWN_FADE_DURATION, 0.0f, 1.0f);
            float t = linearT * linearT;

            uberParams.smoothness = baseSmoothness + (targetSmoothness - baseSmoothness) * t;
            uberParams.intensity = baseIntensity + (targetIntensity - baseIntensity) * t;
        }
        // PRIORITY 2: Handle Falling Fade-Out (Going to Black)
        else if (player)
        {
            float currentY = player->GetPosition().y;

            if (currentY < startFadeY)
            {
                // Calculate Fall Progress (0.0 = Start Fall, 1.0 = Dead)
                float totalDist = startFadeY - killPlaneY;
                float currentFall = startFadeY - currentY;
                float t = std::clamp(currentFall / totalDist, 0.0f, 1.0f);

                // Apply Fade Out
                uberParams.smoothness = baseSmoothness + (targetSmoothness - baseSmoothness) * t;
                uberParams.intensity = baseIntensity + (targetIntensity - baseIntensity) * t;

                // Check for Death
                if (currentY <= (killPlaneY + 1.0f))
                {
                    LoadCheckpoint(); // Reset Position

                    // [FIX] Do NOT snap variables back. 
                    // Start the Fade-In timer instead.
                    m_respawnTimer = RESPAWN_FADE_DURATION;

                    // Force values to remain Black for this specific frame
                    uberParams.smoothness = targetSmoothness;
                    uberParams.intensity = targetIntensity;
                }
            }
            else
            {
                // Normal State (Safe on ground)
                uberParams.smoothness = baseSmoothness;
                uberParams.intensity = baseIntensity;
            }
        }
    }

    if (m_impactDisplay) {
        m_impactDisplay->Update(elapsedTime);
    }

    // =========================================================
        // LOGIC KAMERA SEQUENCE & TRIGGER MASH BERKALA
        // =========================================================
    auto currentCamMode = CameraController::Instance().GetControlMode();
    bool isCameraC = (currentCamMode == CameraControlMode::FixedFollow);

    // 1. UPDATE STATUS (LATCH SYSTEM)
    // Jika masuk Index 1, aktifkan status Mash
    if (seqInfo.CurrentIndex == 1)
    {
        m_mashSequenceActive = true;
    }
    // Jika sudah masuk Kamera C (FixedFollow), matikan paksa
    if (isCameraC)
    {
        m_mashSequenceActive = false;
    }

    // 2. JALANKAN TIMER (Hanya jika status aktif)
    // Kita hapus ketergantungan pada 'seqInfo.IsPlaying'
    if (m_mashSequenceActive)
    {
        // Hitung waktu sendiri (akumulasi)
        m_mashTimer += elapsedTime;

        // Target: Pertama 1.0 detik, selanjutnya 3.0 detik
        float targetTime = m_isMashFirstRun ? 1.0f : 5.0f;

        if (m_mashTimer >= targetTime)
        {
            // Trigger UI Sequence
            std::vector<ImpactEvent> seq = {
                { ImpactType::SpaceKeyJP, 0.8f },
                { ImpactType::RendaSeyo,  1.0f }
            };
            m_impactDisplay->ShowSequence(seq);

            m_mashLoopCount++;

            if (m_mashLoopCount >= 2)
            {
                m_showSubText = true;
            }

            // Reset Timer
            m_mashTimer = 0.0f;
            m_isMashFirstRun = false;
        }
    }
    else
    {
        // Jika status mash tidak aktif (reset atau belum mulai), reset counter juga
        // Supaya nanti kalau mulai lagi, subtext hilang dulu
        if (!m_mashSequenceActive) // Bisa juga cek !seqInfo.CurrentIndex == 1
        {
            m_mashTimer = 0.0f;
            m_isMashFirstRun = true;
            m_mashLoopCount = 0;   // Reset hitungan
            m_showSubText = false; // Sembunyikan subtext
        }
    }
}

void SceneGameBreaker::UpdateGameTriggers(float elapsedTime)
{
    if (m_hasTriggered) return;

    // Trigger Breakout Mode (Tombol T atau blok habis)
    bool triggerCondition = (GetKeyState('T') & 0x8000)
        || (blockManager && blockManager->GetActiveBlockCount() <= Config::TRIGGER_BLOCK_COUNT);

    if (triggerCondition)
    {
        m_hasTriggered = true;
        if (player) player->SetInputEnabled(false);
        if (paddle) paddle->SetAIEnabled(true);

        m_director->TriggerIntroSequence();
    }
}

void SceneGameBreaker::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    m_postProcess->SetEnabled(m_fxState.MasterEnabled);

    if (m_fxState.MasterEnabled) {
        m_postProcess->BeginCapture();
    }
    else {
        ID3D11RenderTargetView* originalRTV = nullptr;
        ID3D11DepthStencilView* originalDSV = nullptr;
        dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);
        if (originalRTV) {
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            dc->ClearRenderTargetView(originalRTV, clearColor);
            originalRTV->Release();
        }
        if (originalDSV) {
            dc->ClearDepthStencilView(originalDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            originalDSV->Release();
        }
    }

    if (m_spriteDEBUG_LAYOUT) {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    }

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    if (m_spriteBorderBreaker && !m_introFinished)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);
        dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

        float finalW = m_spriteSize.x * m_spriteScale;
        float finalH = m_spriteSize.y * m_spriteScale;
        m_spriteBorderBreaker->Render(dc, targetCam, m_spritePos.x, m_spritePos.y, m_spritePos.z,
            finalW, finalH, XMConvertToRadians(m_spritePitch), XMConvertToRadians(m_spriteYaw), XMConvertToRadians(m_spriteRoll + m_bgRotation),
            bgSpriteColor.x, bgSpriteColor.y, bgSpriteColor.z, bgSpriteColor.w);
    }

    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (text && m_modelIntroFinished) {
        dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);
        text->Draw(tutorialText.c_str(), tutorialLayout.x, tutorialLayout.y, tutorialLayout.scale, tutorialLayout.color[0], tutorialLayout.color[1], tutorialLayout.color[2], tutorialLayout.color[3]);
    }

    // DEBUG RENDERING
    if (targetCam == mainCamera.get()) {
        auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
        auto primRenderer = Graphics::Instance().GetPrimitiveRenderer(); // Get PrimitiveRenderer
        if (m_itemManager) m_itemManager->RenderDebug(shapeRenderer);

        // Pass both renderers to Stage
        if (m_stage) m_stage->RenderDebug(shapeRenderer, primRenderer);

        if (m_enemyManager) m_enemyManager->RenderDebug(shapeRenderer);

        shapeRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection());

        // Render Lines
        primRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    if (m_impactDisplay) {
        auto rs = Graphics::Instance().GetRenderState();
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        m_impactDisplay->Render(dc);
    }

    // [BARU] Render SubText
    if (m_spriteSubText && m_showSubText)
    {
        // Pastikan Blend Transparency Aktif (biasanya sudah aktif dari impactDisplay, tapi untuk aman:)
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        // Ukuran Sprite (Sesuai Request)
        float w = 369.0f;
        float h = 36.0f;

        // Posisi: Tengah Bawah
        // X = (Layar / 2) - (Gambar / 2)
        // Y = Layar - (Gambar) - Margin (misal 50 pixel dari bawah)
        float screenW = 1920.0f; // Atau ambil dari GetMainWindow
        float screenH = 1080.0f;

        // Cek update resolusi dynamic jika perlu (ambil dari framework window width/height)
        if (auto win = Framework::Instance()->GetMainWindow()) {
            screenW = (float)win->GetWidth();
            screenH = (float)win->GetHeight();
        }

        float dx = (screenW * 0.5f) - (w * 0.5f);
        float dy = screenH - h - 100.0f; // 100 pixel dari bawah biar agak naik dikit

        // Render (Warna Putih Solid)
        m_spriteSubText->Render(dc, dx, dy, 0.0f, w, h, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Render SubText (Mouse Shift)
    if (m_spriteSubTextMouseShift && m_showDefenseSubtext)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        float w = 278.0f;
        float h = 55.0f;
        float screenW = 1920.0f;
        float screenH = 1080.0f;

        if (auto win = Framework::Instance()->GetMainWindow()) {
            screenW = (float)win->GetWidth();
            screenH = (float)win->GetHeight();
        }

        float dx = (screenW * 0.5f) - (w * 0.5f);
        float dy = screenH - h - 100.0f;

        // --- FADE LOGIC ---
        float alpha = 1.0f;
        if (m_defenseConditionMet)
        {
            float fadeStartTime = 2.5f;

            if (m_defenseStopTimer > fadeStartTime)
            {
                float fadeProgress = m_defenseStopTimer - fadeStartTime;
                alpha = 1.0f - fadeProgress; 
                alpha = (std::max)(0.0f, alpha);
            }
        }
        m_spriteSubTextMouseShift->Render(dc, dx, dy, 0.0f, w, h, 0.0f, 1.0f, 1.0f, 1.0f, alpha);
    }

    // Render Subtext (Mouse Space)
    if (m_spriteSubTextMouseSpace && m_showAttackSubtext)
    {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        float w = 278.0f;
        float h = 55.0f;
        float screenW = 1920.0f;
        float screenH = 1080.0f;

        if (auto win = Framework::Instance()->GetMainWindow()) {
            screenW = (float)win->GetWidth();
            screenH = (float)win->GetHeight();
        }

        float dx = (screenW * 0.5f) - (w * 0.5f);
        float dy = screenH - h - 100.0f;

        // --- FADE LOGIC ---
        float alpha = 1.0f;
        if (m_attackConditionMet)
        {
            float fadeStartTime = 2.5f;

            if (m_attackStopTimer > fadeStartTime)
            {
                float fadeProgress = m_attackStopTimer - fadeStartTime;
                alpha = 1.0f - fadeProgress;
                alpha = (std::max)(0.0f, alpha);
            }
        }

        m_spriteSubTextMouseSpace->Render(dc, dx, dy, 0.0f, w, h, 0.0f, 1.0f, 1.0f, 1.0f, alpha);
    }

    if (m_fxState.MasterEnabled)
    {
        UberShader::UberData& activeData = m_postProcess->GetData();
        activeData = this->uberParams;

        if (!m_fxState.EnableVignette) activeData.intensity = 0.0f;
        if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; }
        if (!m_fxState.EnableChromatic) { activeData.chromaticAberration = 0.0f; }
        if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }

        m_postProcess->EndCapture(elapsedTime);
    }

}

void SceneGameBreaker::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;
    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, &m_lightManager };

    // =========================================================
    // [MODIFIED] SEQUENTIAL INTRO: GRID -> BALL -> PADDLE
    // =========================================================
    if (!m_modelIntroFinished)
    {
        int visibleCount = (int)(m_modelIntroTimer / m_modelIntroSpeed);

        int rows = 7;
        int cols = 7;
        if (blockManager) {
            rows = blockManager->GetRows();
            cols = blockManager->GetColumns();
        }

        int centerZ = rows / 2;
        int centerX = cols / 2;
        int totalGridCells = rows * cols;

        // Clamp limit to total cells so the loop doesn't overflow
        int limit = (std::min)(visibleCount, totalGridCells);

        // 1. Render Grid (Blocks + Player) - Top-Left to Bottom-Right
        if (blockManager)
        {
            auto& blocks = blockManager->GetBlocks();
            DirectX::XMFLOAT4 bColor = blockManager->globalBlockColor;

            for (int k = 0; k < limit; ++k)
            {
                // Map linear 'k' to Visual Grid (Top-Left start)
                int visRow = k / cols;
                int visCol = k % cols;

                // Map Visual Grid to Storage Grid (Bottom-Up Z)
                // Row 0 (Visual Top) -> Z = 6 (Storage Top)
                int storageZ = (rows - 1) - visRow;
                int storageX = visCol;

                // CHECK: Is this the Player Hole?
                if (storageZ == centerZ && storageX == centerX)
                {
                    // Render Player EXACTLY when the scan hits the middle
                    if (player) modelRenderer->Draw(ShaderId::Phong, player->GetModel(), player->color);
                }
                else
                {
                    // Calculate Index in 'blocks' vector
                    // Vector is filled Bottom-Up, Left-Right. 
                    // We must account for the missing hole in the index calculation.
                    int linearIdx = storageZ * cols + storageX;
                    if (storageZ > centerZ || (storageZ == centerZ && storageX > centerX))
                    {
                        linearIdx--; // Shift back 1 because the hole wasn't added to the vector
                    }

                    if (linearIdx >= 0 && linearIdx < blocks.size())
                    {
                        if (blocks[linearIdx]->IsActive())
                            blocks[linearIdx]->Render(modelRenderer, bColor);
                    }
                }
            }
        }

        // 2. Render Ball (After Grid)
        // Delay: 5 frames after grid finishes
        if (ball && visibleCount > totalGridCells + 5)
        {
            ball->Render(modelRenderer);
        }

        // 3. Render Paddle (After Ball)
        // Delay: 10 frames after grid finishes (so 5 frames after Ball)
        if (paddle && paddle->IsActive())
        {
            if (visibleCount > totalGridCells + 10)
            {
                modelRenderer->Draw(ShaderId::Phong, paddle->GetModel(), paddle->color);
            }
        }

        // 4. Finish Animation State
        // Give a small buffer before enabling the text
        if (visibleCount > totalGridCells + 20)
        {
            m_modelIntroFinished = true;
        }
    }
    else
    {
        // =========================================================
        // NORMAL RENDERING (OPTIMIZED)
        // =========================================================
        if (ball) ball->Render(modelRenderer);
        if (blockManager) blockManager->Render(modelRenderer);
        if (paddle && paddle->IsActive()) modelRenderer->Draw(ShaderId::Phong, paddle->GetModel(), paddle->color);
        if (player) modelRenderer->Draw(ShaderId::Phong, player->GetModel(), player->color);
    }

    // Always render these normally
    if (m_introFinished && m_enemyManager) { m_enemyManager->Render(modelRenderer); }
    if (m_introFinished && m_itemManager) m_itemManager->Render(modelRenderer);
    if (m_isShakeEnabled && m_stage) { m_stage->UpdateTransform(); m_stage->Render(modelRenderer); }

    modelRenderer->Render(rc);
}

void SceneGameBreaker::SaveCheckpoint(const DirectX::XMFLOAT3& checkpointPos)
{
    float dx = m_checkpoint.position.x - checkpointPos.x;
    float dz = m_checkpoint.position.z - checkpointPos.z;
    if (m_checkpoint.isValid && (dx * dx + dz * dz < 0.1f)) return;
    if (player && blockManager)
    {
        m_checkpoint.position = { checkpointPos.x, 0.0f, checkpointPos.z };
        m_checkpoint.savedBlockCount = blockManager->GetActiveBlockCount();
        m_checkpoint.savedGameStage = player->GetGameStage();
        m_checkpoint.savedCanShield = player->CanUseShield();
        m_checkpoint.savedCanShoot = player->CanShoot();
        m_checkpoint.isValid = true;
    }
}

void SceneGameBreaker::StartPlayerDeathSequence()
{
    if (m_isDying) return;
    m_isDying = true;
    m_deathTimer = 0.0f;

    if (player)
    {
        player->SetInputEnabled(false);
        player->scale = { 0.0f, 0.0f, 0.0f };
    }
}

void SceneGameBreaker::LoadCheckpoint()
{
    if (!m_checkpoint.isValid)
    {
        if (player) player->SetPosition(0, 0, 0);
        return;
    }

    if (player)
    {
        player->SetPosition(m_checkpoint.position);
        player->GetMovement()->SetVelocity({ 0, 0, 0 });
        player->SetFalling(false);
        player->SetInputEnabled(true);
        player->SetGameStage(m_checkpoint.savedGameStage);
        player->GetMovement()->SetRotationY(DirectX::XM_PI);
        player->SetAbilityShield(m_checkpoint.savedCanShield);
        player->SetAbilityShoot(m_checkpoint.savedCanShoot);
        player->scale = { 3.0f, 3.0f, 3.0f };
    }

    if (m_enemyManager)
    {
        m_enemyManager->GetEnemies().clear();
        m_enemyManager->Initialize(Graphics::Instance().GetDevice());
    }

    if (m_itemManager)
    {
        m_itemManager->GetItems().clear();
        m_itemManager->Initialize(Graphics::Instance().GetDevice());
    }

    if (blockManager)
    {
        blockManager->RestoreShield(m_checkpoint.savedBlockCount, player);
    }

    m_hasTriggeredEscapeSequence = false;
    // [RESET MASH TIMER]
    m_mashTimer = 0.0f;
    m_isMashFirstRun = true;
    m_mashSequenceActive = false;
    m_showSubText = false;
    m_mashLoopCount = 0;

    // Reset Flags
    m_isDying = false;
    m_deathTimer = 0.0f;

    // Start Fade-In 
    m_respawnTimer = RESPAWN_FADE_DURATION;
}

void SceneGameBreaker::DrawGUI()
{
    GameBreakerGUI::Draw(this);
}

void SceneGameBreaker::OnResize(int width, int height)
{
    if (height <= 0) height = 1;

    if (mainCamera)
    {
        mainCamera->SetPerspectiveFov(
            DirectX::XMConvertToRadians(Config::CAM_FOV),
            (float)width / (float)height,
            Config::CAM_NEAR,
            Config::CAM_FAR
        );
    }

    if (m_postProcess)
    {
        m_postProcess->OnResize(width, height);
    }
}

DirectX::XMFLOAT3 SceneGameBreaker::GetMouseOnGround(Camera* camera)
{
    if (!camera) return { 0,0,0 };

    auto& input = Input::Instance();
    int mx = input.GetMouse().GetPositionX();
    int my = input.GetMouse().GetPositionY();

    auto window = Framework::Instance()->GetMainWindow();
    float w = (float)window->GetWidth();
    float h = (float)window->GetHeight();

    float ndcX = (2.0f * mx) / w - 1.0f;
    float ndcY = 1.0f - (2.0f * my) / h;

    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&camera->GetProjection());
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&camera->GetView());
    DirectX::XMMATRIX InvVP = DirectX::XMMatrixInverse(nullptr, V * P);

    DirectX::XMVECTOR nearPoint = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), InvVP);
    DirectX::XMVECTOR farPoint = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), InvVP);

    DirectX::XMVECTOR directionVec = DirectX::XMVectorSubtract(farPoint, nearPoint);
    DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(directionVec);

    DirectX::XMFLOAT3 rayOrigin, rayDir;
    DirectX::XMStoreFloat3(&rayOrigin, nearPoint);
    DirectX::XMStoreFloat3(&rayDir, dir);

    if (std::abs(rayDir.y) < 1e-6) return { rayOrigin.x, 0, rayOrigin.z };

    float t = -rayOrigin.y / rayDir.y;

    return {
        rayOrigin.x + rayDir.x * t,
        0.0f,
        rayOrigin.z + rayDir.z * t
    };
}