#include "SceneTitle.h"

SceneTitle::SceneTitle()
{
    if (auto window{ Framework::Instance()->GetMainWindow() }) {
        SDL_Window* sdlWin = window->GetSDLWindow();

        // Disable window borders and the ability to resize
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowResizable(sdlWin, false);

        // Grab monitor size and force the window to match it perfectly
        int fullW = GetSystemMetrics(SM_CXSCREEN);
        int fullH = GetSystemMetrics(SM_CYSCREEN);
        SDL_SetWindowSize(sdlWin, fullW, fullH);
        SDL_SetWindowPosition(sdlWin, 0, 0);
    }

    // 1. Initialize Core Systems
    camera = std::make_unique<Camera>();
    camera->SetOrthographic(1920.0f, 1080.0f, 0.1f, 1000.0f);
    camera->SetPosition(0.0f, 0.0f, -10.0f);

    postProcess = std::make_unique<PostProcessManager>();
    postProcess->Initialize(1920, 1080);

    m_uberParams.center = { 0.5f, 0.5f };     
    m_uberParams.roundness = 1.0f;            

    // Standar Resolusi PSX (Bisa diturunin misal ke 640x480 kalo mau lebih retro)
    m_uberParams.psxResWidth = 1920.0f;
    m_uberParams.psxResHeight = 1080.0f;
    m_uberParams.psxColorDepth = 256.0f;      
    m_uberParams.psxDitherStrength = 1.0f;

    // Load Assets & Finalize Setup
    bgSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Title/Back_Title.png");
    logoSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Title/Sprite_Title_Logo.png");
    copyrightSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Title/Sprite_Title_Copyright.png");
    startSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Title/Sprite_Title_Start.png");
    m_fadeSprite = std::make_unique<Sprite>(Graphics::Instance().GetDevice(), "Data/Sprite/Scene Game/Black.png");
}

// =========================================================
// CORE LOOP
// =========================================================

void SceneTitle::Update(float elapsedTime)
{
    // =========================================================
    // 1. EXIT PHASE (Triggered by Enter key)
    // =========================================================
    if (m_isExiting)
    {
        m_exitTimer += elapsedTime;

        // Fade to black smoothly over BOOT_FADE_DURATION
        m_fadeAlpha = std::clamp(m_exitTimer / BOOT_FADE_DURATION, 0.0f, 1.0f);

        // Once fully black, change the scene!
        if (m_exitTimer >= BOOT_FADE_DURATION)
        {
            Framework::Instance()->ChangeScene(std::make_unique<SceneGame>());
        }

        return; // Stop updating the menu while fading out
    }

    // =========================================================
    // 2. NORMAL BOOT & TITLE PHASE
    // =========================================================
    if (m_bootTimer > 0.0f)
    {
        m_bootTimer -= elapsedTime;
        m_fadeAlpha = std::clamp(m_bootTimer / BOOT_FADE_DURATION, 0.0f, 1.0f);
    }
    // SHOW COPYRIGHT (Wait Phase)
    else if (m_copyrightTimer > 0.0f)
    {
        m_fadeAlpha = 0.0f; // Screen is fully visible
        m_copyrightTimer -= elapsedTime;
    }
    else
    {
        m_fadeAlpha = 0.0f;

        // Fade out Copyright
        if (m_copyrightAlpha > 0.0f)
            m_copyrightAlpha = std::clamp(m_copyrightAlpha - (elapsedTime * 2.0f), 0.0f, 1.0f);

        // Phase 1: Fade In (Only runs while pulseTimer is 0)
        if (m_pulseTimer == 0.0f)
        {
            m_startAlpha += elapsedTime * 2.0f;
            if (m_startAlpha >= 1.0f)
            {
                m_startAlpha = 1.0f;
                // Nudge the timer slightly above 0 so we enter Phase 2 on the next frame
                m_pulseTimer += 0.001f;
            }
        }
        // Phase 2: Pulse Effect
        else
        {
            m_pulseTimer += elapsedTime;
            m_startAlpha = 0.6f + 0.4f * sinf(m_pulseTimer * 3.0f);

            // ALLOW INPUT ONLY WHEN START TEXT IS PULSING
            if (Input::Instance().GetKeyboard().IsTriggered(VK_RETURN))
            {
                m_isExiting = true;
                m_exitTimer = 0.0f;

                // You can play your Enter SFX here if you have one!
                // AudioManager::Instance().PlaySFX("Data/Sound/SE_Start.wav");
            }
        }
    }
    m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
    m_uberParams.intensity = FX_BASE_INTENSITY;
}

void SceneTitle::Render(float dt, Camera * targetCamera)
{
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    // ==============================================================
    // --- STEP 1: Sinkronisasi Data Post-Process (Harus di Atas!) ---
    // ==============================================================
    postProcess->SetEnabled(m_fxState.MasterEnabled);

    UberShader::UberData& activeData = postProcess->GetData();
    activeData = this->m_uberParams; // <-- Pastikan pakai m_uberParams

    // Evaluasi Flags & Masking Efek
    activeData.psxEnabled = (m_fxState.MasterEnabled && m_fxState.EnablePSX);
    if (!m_fxState.EnableVignette)   activeData.intensity = 0.0f;
    if (!m_fxState.EnableLens) { activeData.glitchStrength = 0.0f; activeData.distortion = 0.0f; activeData.blurStrength = 0.0f; }
    if (!m_fxState.EnableChromatic)  activeData.chromaticAberration = 0.0f;
    if (!m_fxState.EnableCRT) { activeData.scanlineStrength = 0.0f; activeData.fineOpacity = 0.0f; }
    if (!m_fxState.EnableBloom)      activeData.bloomIntensity = 0.0f;

    // ==============================================================
    // --- STEP 2: Mulai Capture ---
    // ==============================================================
    if (m_fxState.MasterEnabled)
    {
        postProcess->BeginCapture();
        postProcess->EndCapture(dt);
    }
    else
    {
        // Fallback: Manually clear the active BackBuffer
        ID3D11RenderTargetView* currentRTV = nullptr;
        ID3D11DepthStencilView* currentDSV = nullptr;
        dc->OMGetRenderTargets(1, &currentRTV, &currentDSV);

        if (currentRTV) {
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            dc->ClearRenderTargetView(currentRTV, clearColor);
            currentRTV->Release();
        }
        if (currentDSV) {
            dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            currentDSV->Release();
        }
    }

    // ==============================================================
    // --- STEP 3: UI & Background Pass ---
    // ==============================================================
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

    float t = 1.0f - std::clamp(m_bootTimer / BOOT_FADE_DURATION, 0.0f, 1.0f);

    // 2. Apply a curve to the alpha. 
    // t * t creates an "Ease-In" effect (starts very slow).
    // If you want it even slower, use t * t * t
    float bootAlpha = t * t;

    // 3. Render your background, logo, and copyright with this alpha
    if (bgSprite)
    {
        bgSprite->Render(dc, camera.get(), 0, 0, 0, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, bootAlpha);
    }

    if (logoSprite)
    {
        logoSprite->Render(dc, 461.5f, 200.0f, 0.0f, 997.0f, 547.0f, 0.0f, 0.0f, 997.0f, 547.0f, 0.0f, 1.0f, 1.0f, 1.0f, bootAlpha);
    }

    float finalCopyrightAlpha = (m_bootTimer > 0.0f) ? bootAlpha : m_copyrightAlpha;

    if (copyrightSprite && finalCopyrightAlpha > 0.0f)
    {
        copyrightSprite->Render(dc, 327.5f, 867.0f, 0.0f, 1265.0f, 105.0f, 0.0f, 0.0f, 1265.0f, 105.0f, 0.0f, 1.0f, 1.0f, 1.0f, finalCopyrightAlpha);
    }

    // RENDER START
    if (startSprite && m_startAlpha > 0.0f)
    {
        startSprite->Render(dc, 739.5f, 906.5f, 0.0f, 441.0f, 26.0f, 0.0f, 0.0f, 441.0f, 26.0f, 0.0f, 1.0f, 1.0f, 1.0f, m_startAlpha);
    }

    if (m_fadeAlpha > 0.001f && m_fadeSprite)
    {
        // Get screen size
        float screenW = 1920.0f; 
        float screenH = 1080.0f;

        m_fadeSprite->Render(
            dc,
            0.0f, 0.0f, 0.0f,          // x, y, z
            screenW, screenH,          // width, height
            0.0f, 0.0f,                // sx, sy
            1920.0f, 1080.0f,          // sw, sh
            0.0f,                      // angle
            0.0f, 0.0f, 0.0f, m_fadeAlpha // r, g, b, a (Fade alpha here)
        );
    }

    if (m_fxState.MasterEnabled)
    {
        postProcess->EndCapture(dt);
    }
}

void SceneTitle::OnResize(int width, int height)
{
    if (postProcess) postProcess->OnResize(width, height);
}

// =========================================================
// DEBUG GUI IMPLEMENTATION
// =========================================================

void SceneTitle::DrawGUI()
{
    ImGui::Begin("Title Scene Debugger");

    if (ImGui::BeginTabBar("InspectorTabs"))
    {
        if (ImGui::BeginTabItem("Post-Process & FX"))
        {
            GUIPostProcessTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void SceneTitle::GUIPostProcessTab()
{
    ImGui::Spacing();

    // Master Switch
    const char* label = m_fxState.MasterEnabled ? "Turn Off Filter" : "Turn On Filter";
    ImVec4 color = m_fxState.MasterEnabled ? ImVec4(0.6f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 0.2f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    if (ImGui::Button(label, ImVec2(-1, 40))) m_fxState.MasterEnabled = !m_fxState.MasterEnabled;
    ImGui::PopStyleColor();

    if (!m_fxState.MasterEnabled) return;

    ImGui::Separator();

    auto CheckboxLayer = [&](const char* label, bool& val) {
        ImGui::Checkbox(label, &val);
        if (!val) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        };

    // Vignette
    if (ImGui::CollapsingHeader("Vignette & Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        CheckboxLayer("ACTIVATE: Vignette", m_fxState.EnableVignette);
        ImGui::ColorEdit3("Tint", &m_uberParams.color.x);
        ImGui::SliderFloat("Intensity", &m_uberParams.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &m_uberParams.smoothness, 0.01f, 1.0f);
        ImGui::Checkbox("Rounded", &m_uberParams.rounded);
        if (!m_fxState.EnableVignette) ImGui::PopStyleVar();
    }

    // Lens
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        CheckboxLayer("ACTIVATE: Lens", m_fxState.EnableLens);
        ImGui::SliderFloat("Fisheye", &m_uberParams.distortion, -0.5f, 0.5f);
        ImGui::SliderFloat("Chroma", &m_uberParams.blurStrength, 0.0f, 0.05f);
        ImGui::SliderFloat("Glitch", &m_uberParams.glitchStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableLens) ImGui::PopStyleVar();
    }

    // CRT
    if (ImGui::CollapsingHeader("CRT Monitor"))
    {
        CheckboxLayer("ACTIVATE: CRT", m_fxState.EnableCRT);
        ImGui::SliderFloat("Density", &m_uberParams.fineDensity, 10.0f, 500.0f);
        ImGui::SliderFloat("Opacity", &m_uberParams.fineOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Speed", &m_uberParams.scanlineSpeed, -10.0f, 10.0f);
        ImGui::SliderFloat("Scan Opacity", &m_uberParams.scanlineStrength, 0.0f, 1.0f);
        if (!m_fxState.EnableCRT) ImGui::PopStyleVar();
    }
}