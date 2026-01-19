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
    //m_spriteDEBUG_LAYOUT = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), pathDebugLayout);
    m_stage = std::make_unique<Stage>(Graphics::Instance().GetDevice());

    ball = new Ball();
    paddle = new Paddle();
    paddle->SetAIEnabled(false);
    player = new Player();
    player->SetInputEnabled(false);
    player->GetMovement()->SetRotationY(DirectX::XM_PI);
    blockManager = std::make_unique<BlockManager>();
    blockManager->Initialize(player);

    m_enemyManager = std::make_unique<EnemyManager>();
    m_enemyManager->Initialize(Graphics::Instance().GetDevice());

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

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    // Update Entities
    if (paddle)
    {
        if (ball) paddle->UpdateAI(elapsedTime, ball, blockManager.get());
        paddle->Update(elapsedTime, activeCam);
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
    }

    if (blockManager)
    {
        blockManager->Update(elapsedTime, activeCam, player);
        if (ball && ball->IsActive())
        {
            blockManager->CheckCollision(ball);
        }
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
        // Semua logika kamera formation & destruction pindah ke sini
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
            player->TriggerEscape();
            CameraController::Instance().SetControlMode(CameraControlMode::FixedFollow);
        }

        if (player->IsEscaping() || player->GetGameStage() == 3)
        {
            DirectX::XMFLOAT3 pPos = player->GetPosition();
            CameraController::Instance().SetTarget(pPos);
        }
    }

    if (m_enemyManager)
    {
        m_enemyManager->Update(elapsedTime, activeCam);
    }

    // Update Systems
    CameraController::Instance().Update(elapsedTime);
    JuiceEngine::Instance().Update(elapsedTime);
    UpdateGameTriggers(elapsedTime);

    // =========================================================
    // LOGIKA SCANLINE ZOOM EFFECT (Dynamic FineDensity)
    // =========================================================

    auto seqInfo = CameraController::Instance().GetSequenceProgress();

    // 1. LOCK STATE
    if (seqInfo.IsPlaying && seqInfo.CurrentIndex >= 1)
    {
        m_isShakeEnabled = true;
    }

    // [MODIFIKASI] Ambil nilai dari variabel Class (Config), bukan hardcode lokal!
    float normalDensity = m_configFineDensity; // Nilai dari ImGui
    float zoomDensity = m_configZoomDensity;   // Nilai dari ImGui
    float baseStrength = Config::FX_CRT_BASE_STRENGTH;

    // Config Rotasi
    float startRotation = 0.0f;
    float targetRotation = Config::FX_CRT_ROTATION_TARGET;

    bool isInsideProgram = m_isShakeEnabled;

    if (isInsideProgram)
    {
        // DALAM PROGRAM: Hilang
        uberParams.fineOpacity = 0.0f;
        uberParams.scanlineStrength = 0.0f;
        uberParams.fineRotation = targetRotation;
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
        uberParams.fineDensity = normalDensity; // Ini akan ngikutin slider ImGui secara realtime
        uberParams.fineRotation = startRotation;
        uberParams.scanlineStrength = baseStrength;
    }

    // Auto Glitch Transition (Optional: Bisa dipindah ke Director juga kalau mau lebih bersih)
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
        // LOGIKA RESET (JALAN HANYA SEKALI SAAT SEQUENCE BERHENTI)
        if (wasPlayingSequence)
        {
            // Sequence baru saja mati frame ini, paksa glitch jadi 0
            if (m_fxState.MasterEnabled && m_fxState.EnableLens)
            {
                uberParams.glitchStrength = 0.0f;
            }
            wasPlayingSequence = false; // Reset flag biar gak ganggu manual slider
            m_introFinished = true;
        }

        if (m_introFinished && m_fxState.MasterEnabled && m_fxState.EnableLens)
        {
            uberParams.glitchStrength = 0.0f;
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
        if (paddle) paddle->SetAIEnabled(true);
        if (player) {
            player->SetBreakoutMode(true);
            player->SetInputEnabled(false);
        }

        // Panggil Director untuk mainkan sequence intro lagi (atau sequence lain)
        // Di sini saya pakai intro lagi sesuai logika lama
        m_director->TriggerIntroSequence();
    }
}

void SceneGameBreaker::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // =========================================================
    // BEGIN POST PROCESS CAPTURE
    // =========================================================
    m_postProcess->SetEnabled(m_fxState.MasterEnabled);

    // If PostProcess is enabled, this swaps RTV to internal texture
    // If disabled, this simply clears the BackBuffer
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

    // =========================================================
    // 2. RENDER SCENE CONTENTS
    // =========================================================
    // Transparent Sprite (Debug Layout)
    if (m_spriteDEBUG_LAYOUT) {
        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    }

    // 3D Objects
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));
    RenderScene(elapsedTime, targetCam);

    // --- World Space Sprite (Transparent) ---
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

    // Text
    BitmapFont* text = ResourceManager::Instance().GetFont("VGA_FONT");
    if (text) {
        dc->OMSetBlendState(Graphics::Instance().GetAlphaBlendState(), nullptr, 0xFFFFFFFF);
        text->Draw(tutorialText.c_str(), tutorialLayout.x, tutorialLayout.y, tutorialLayout.scale, tutorialLayout.color[0], tutorialLayout.color[1], tutorialLayout.color[2], tutorialLayout.color[3]);
    }

    // Debug Shapes
    if (targetCam == mainCamera.get()) {
        Graphics::Instance().GetShapeRenderer()->Render(dc, targetCam->GetView(), targetCam->GetProjection());
    }

    // =========================================================
    // END POST PROCESS CAPTURE (DRAW TO SCREEN)
    // =========================================================
    if (m_fxState.MasterEnabled)
    {
        UberShader::UberData& activeData = m_postProcess->GetData();
        activeData = this->uberParams; 

        // Apply visual toggles
        if (!m_fxState.EnableVignette) activeData.intensity = 0.0f;
        if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; }
        if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }

        // Draw the fullscreen quad with effects
        m_postProcess->EndCapture(elapsedTime);
    }
}

void SceneGameBreaker::RenderScene(float elapsedTime, Camera* camera)
{
    if (!camera) return;
    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, &m_lightManager };

    if (ball) ball->Render(modelRenderer);
    if (blockManager) blockManager->Render(modelRenderer);
    if (paddle && paddle->IsActive()) modelRenderer->Draw(ShaderId::Phong, paddle->GetModel(), paddle->color);
    if (player) modelRenderer->Draw(ShaderId::Phong, player->GetModel(), player->color);
    if (m_introFinished && m_enemyManager) { m_enemyManager->Render(modelRenderer); }
    if (m_introFinished && m_stage) { m_stage->UpdateTransform(); m_stage->Render(modelRenderer); }
    modelRenderer->Render(rc);
}

void SceneGameBreaker::DrawGUI()
{
    // Hanya satu baris ini sekarang!
    GameBreakerGUI::Draw(this);
}

void SceneGameBreaker::OnResize(int width, int height)
{
    // Mencegah pembagian dengan nol
    if (height <= 0) height = 1;

    // 1. Update Aspect Ratio Kamera Utama
    if (mainCamera)
    {
        // Update FOV dengan aspect ratio baru (Width / Height)
        mainCamera->SetPerspectiveFov(
            DirectX::XMConvertToRadians(Config::CAM_FOV),
            (float)width / (float)height,
            Config::CAM_NEAR,
            Config::CAM_FAR
        );
    }

    // 2. Buat Ulang Render Target (Texture layar)
    // Penting! Kalau tidak dipanggil, Post-Process akan error atau gepeng saat window di-resize.
    if (m_postProcess)
    {
        m_postProcess->OnResize(width, height);
    }
}