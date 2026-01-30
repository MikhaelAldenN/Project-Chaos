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

    m_collisionManager->SetOnPlayerDeathCallback([this](){ LoadCheckpoint(); });
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
        m_enemyManager->Update(elapsedTime, activeCam, targetPos);
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
    if (text) {
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

    if (ball) ball->Render(modelRenderer);
    if (blockManager) blockManager->Render(modelRenderer);
    if (paddle && paddle->IsActive()) modelRenderer->Draw(ShaderId::Phong, paddle->GetModel(), paddle->color);
    if (player) 
    {
        modelRenderer->Draw(ShaderId::Phong, player->GetModel(), player->color);
    }
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
        m_checkpoint.isValid = true;

        printf("CHECKPOINT REACHED! Pos: %.1f, %.1f | Blocks: %d\n",
            checkpointPos.x, checkpointPos.z, m_checkpoint.savedBlockCount);
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