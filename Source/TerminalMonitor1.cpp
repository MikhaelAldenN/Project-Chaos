#include "TerminalMonitor1.h"
#include "System/Graphics.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

TerminalMonitor1::TerminalMonitor1() {}
TerminalMonitor1::~TerminalMonitor1() {}

void TerminalMonitor1::Initialize(ID3D11Device* device, int width, int height)
{
    m_width = width;
    m_height = height;

    // ... (Setup Texture SAMA) ...
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    device->CreateTexture2D(&texDesc, nullptr, m_renderTargetTexture.GetAddressOf());
    device->CreateRenderTargetView(m_renderTargetTexture.Get(), nullptr, m_rtv.GetAddressOf());
    device->CreateShaderResourceView(m_renderTargetTexture.Get(), nullptr, m_srv.GetAddressOf());

    m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };

    // Components
    m_primitive = std::make_unique<Primitive>(device);
    m_cursor = std::make_unique<CursorBlock>(device);

    // Load Sprite Lock
    m_lockBodySprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Beyond/Sprite_LockBody.png");
    m_lockHeadSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Beyond/Sprite_LockHead.png");

    // Init Cursor
    m_cursor->Initialize(m_cursorBaseW, m_cursorBaseH);
    m_cursor->SetBlink(true, 0.8f, 0.5f);
    m_cursor->SetGridSnap(false, 0, 0);
    // [FIX] Set warna awal
    m_cursor->SetColor(m_cursorColor.x, m_cursorColor.y, m_cursorColor.z, m_cursorColor.w);

    m_cursorPos = { (float)width / 2.0f - (m_cursorBaseW / 2.0f), (float)height / 2.0f - (m_cursorBaseH / 2.0f) };


    // ==========================================
    // [UPDATE] SETTINGS SESUAI SCREENSHOT
    // ==========================================

    // Center Pos (256, 256)
    m_lockPos = { 256.0f, 256.0f };

    // Scale Settings (Separate)
    m_lockHeadScale = 0.6f;
    m_lockBodyScale = 0.6f;

    // Animation Offsets
    // HEAD (Top)
    m_headStartOffsetY = -85.0f;
    m_headEndOffsetY = -45.0f;

    // BODY (Bottom)
    m_bodyStartOffsetY = 47.0f;
    m_bodyEndOffsetY = 20.0f;

    // Duration
    m_lockAnimDuration = 0.4f;
}

// ... (ShowLockScreen, PlayCommandAnimation, Update -> SAMA SEPERTI SEBELUMNYA) ...
// Pastikan Update Cursor Logic juga tidak berubah
void TerminalMonitor1::ShowLockScreen()
{
    m_animState = TerminalAnimState::SYSTEM_LOCK;
    m_currentDisplay = "";
    m_displayLines.clear();

    m_lockAnimTimer = 0.0f;
    m_manualDebugControl = false;

    m_currentHeadY = m_headStartOffsetY;
    m_currentBodyY = m_bodyStartOffsetY;
}

void TerminalMonitor1::PlayCommandAnimation(const std::string& command)
{
    m_targetCommand = command;
    m_currentDisplay = "";
    m_displayLines.clear();
    m_animState = TerminalAnimState::MOVING_TO_POS;
    m_animTimer = 0.0f;
    m_cursor->SetBlink(true, 99.0f, 0.0f);
}

bool TerminalMonitor1::IsBusy() const
{
    return m_animState != TerminalAnimState::IDLE && m_animState != TerminalAnimState::DONE;
}

void TerminalMonitor1::ResetToIdle()
{
    m_animState = TerminalAnimState::IDLE;

    // Bersihkan teks dan cache
    m_targetCommand = "";
    m_currentDisplay = "";
    m_displayLines.clear();

    m_animTimer = 0.0f;

    // Kembalikan kedipan kursor ke normal (santai)
    if (m_cursor)
    {
        m_cursor->SetBlink(true, 0.8f, 0.5f);
    }
}

void TerminalMonitor1::Update(float dt)
{
    // ... Copy Logic Update dari file sebelumnya (Lerp Scale, Switch Case) ...
    // Bagian ini tidak berubah dari kode "Lerp" terakhir

    // Singkatnya: Update logic cursor, typing, dll.
    float centerX = (m_width / 2.0f) - (m_cursorBaseW / 2.0f);
    float centerY = (m_height / 2.0f) - (m_cursorBaseH / 2.0f);

    float targetScale = (m_animState == TerminalAnimState::IDLE) ? 1.0f : 0.8f;
    float lerpSpeed = 15.0f;
    m_currentScale += (targetScale - m_currentScale) * lerpSpeed * dt;

    if (m_cursor)
    {
        m_cursor->SetSize(m_cursorBaseW * m_currentScale, m_cursorBaseH * m_currentScale);
    }

    switch (m_animState)
    {
    case TerminalAnimState::IDLE:
        m_cursorPos.x += (centerX - m_cursorPos.x) * 5.0f * dt;
        m_cursorPos.y += (centerY - m_cursorPos.y) * 5.0f * dt;
        break;

    case TerminalAnimState::MOVING_TO_POS:
    {
        float speed = 10.0f;
        float distX = m_typingStartPos.x - m_cursorPos.x;
        float distY = m_typingStartPos.y - m_cursorPos.y;
        m_cursorPos.x += distX * speed * dt;
        m_cursorPos.y += distY * speed * dt;
        if (std::abs(distX) < 1.0f && std::abs(distY) < 1.0f) {
            m_cursorPos = m_typingStartPos;
            m_animState = TerminalAnimState::TYPING;
            m_animTimer = 0.0f;
            m_cursor->SetBlink(true, 0.1f, 0.05f);
        }
    }
    break;

    case TerminalAnimState::TYPING:
        m_animTimer += dt;
        if (m_animTimer >= m_typeSpeed) {
            m_animTimer = 0.0f;
            if (m_currentDisplay.length() < m_targetCommand.length()) {
                char nextChar = m_targetCommand[m_currentDisplay.length()];
                m_currentDisplay += nextChar;
                if (m_displayLines.empty() || m_displayLines.back().length() >= m_maxCharsPerLine)
                    m_displayLines.push_back(std::string(1, nextChar));
                else m_displayLines.back() += nextChar;

                int row = (int)m_displayLines.size() - 1;
                int col = (int)m_displayLines.back().length();
                m_cursorPos.x = m_typingStartPos.x + (col * m_charWidth) + m_cursorOffsetX;
                m_cursorPos.y = m_typingStartPos.y + (row * m_lineSpacing);
            }
            else {
                m_animState = TerminalAnimState::DONE;
                m_animTimer = 0.0f;
                m_cursor->SetBlink(true, 0.5f, 0.5f);
            }
        }
        break;
    case TerminalAnimState::DONE: break;
    case TerminalAnimState::SYSTEM_LOCK:
        // [PERBAIKAN LOGIKA PLAY ANIMATION]

        float t = 0.0f;

        // Jika Manual Control AKTIF -> Pakai Slider
        if (m_manualDebugControl)
        {
            t = m_debugAnimProgress;
        }
        else
        {
            // Jika TIDAK -> Jalan otomatis pakai Timer
            m_lockAnimTimer += dt;
            t = m_lockAnimTimer / m_lockAnimDuration;
            if (t > 1.0f) t = 1.0f;

            // Sinkronkan slider debug agar visualnya ikut bergerak
            m_debugAnimProgress = t;
        }

        // Easing Function (SmoothStep)
        float smoothT = t * t * (3.0f - 2.0f * t);

        // Lerp Posisi
        m_currentHeadY = m_headStartOffsetY + (m_headEndOffsetY - m_headStartOffsetY) * smoothT;
        m_currentBodyY = m_bodyStartOffsetY + (m_bodyEndOffsetY - m_bodyStartOffsetY) * smoothT;
        break;
    }
    UpdateCursorLogic(dt);
}

void TerminalMonitor1::UpdateCursorLogic(float dt)
{
    if (m_cursor) m_cursor->Update(dt, m_cursorPos.x, m_cursorPos.y);
}


void TerminalMonitor1::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
{
    if (!font) return;

    // Backup & Setup RTT (Sama)
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    D3D11_VIEWPORT prevVP;
    UINT numVP = 1;
    context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    context->RSGetViewports(&numVP, &prevVP);

    ID3D11RenderTargetView* rtv = m_rtv.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->RSSetViewports(1, &m_viewport);
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(rtv, clearColor);

    auto rs = Graphics::Instance().GetRenderState();
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);

    // ============================================
    // [LOGIC RENDER SPRITE]
    // Muncul jika State LOCK aktif ATAU Debug Mode nyala
    // ============================================
    bool showSprite = (m_animState == TerminalAnimState::SYSTEM_LOCK) || m_debugShowLock;

    if (showSprite)
    {
        // [UPDATE] MENGGUNAKAN UKURAN ASLI (PIXEL)

        // 1. Render Body (Bawah) - Asli: 296 x 255
        if (m_lockBodySprite)
        {
            float baseW = 296.0f;
            float baseH = 255.0f;

            // Hitung ukuran akhir berdasarkan Scale Slider
            float finalW = baseW * m_lockBodyScale;
            float finalH = baseH * m_lockBodyScale;

            // Pivot tengah
            float pivotX = m_lockPos.x - (finalW / 2.0f);
            float pivotY = m_lockPos.y - (finalH / 2.0f);

            // Posisi Y + Offset Animasi
            float drawY = pivotY + m_currentBodyY;

            m_lockBodySprite->Render(context,
                pivotX, drawY, 0.0f,
                finalW, finalH,
                0.0f,
                m_lockColor.x, m_lockColor.y, m_lockColor.z, m_lockColor.w);
        }

        // 2. Render Head (Atas) - Asli: 238 x 348
        if (m_lockHeadSprite)
        {
            float baseW = 238.0f;
            float baseH = 348.0f;

            // Hitung ukuran akhir berdasarkan Scale Slider
            float finalW = baseW * m_lockHeadScale;
            float finalH = baseH * m_lockHeadScale;

            // Pivot tengah
            float pivotX = m_lockPos.x - (finalW / 2.0f);
            float pivotY = m_lockPos.y - (finalH / 2.0f);

            // Posisi Y + Offset Animasi
            float drawY = pivotY + m_currentHeadY;

            m_lockHeadSprite->Render(context,
                pivotX, drawY, 0.0f,
                finalW, finalH,
                0.0f,
                m_lockColor.x, m_lockColor.y, m_lockColor.z, m_lockColor.w);
        }
    }

    // ============================================
    // [LOGIC RENDER TEXT & CURSOR]
    // Tampilkan jika Sprite TIDAK aktif, ATAU jika kita mau debug numpuk (opsional)
    // Di sini saya buat Text hilang jika Lock Screen aktif (supaya clean)
    // ============================================
    if (!showSprite)
    {
        if (m_animState == TerminalAnimState::TYPING || m_animState == TerminalAnimState::DONE)
        {
            for (size_t i = 0; i < m_displayLines.size(); ++i)
            {
                font->Draw(
                    m_displayLines[i].c_str(),
                    m_typingStartPos.x,
                    m_typingStartPos.y + (i * m_lineSpacing),
                    m_fontScale,
                    1.0f, 0.2f, 0.2f, 1.0f
                );
            }
        }

        if (m_cursor && m_primitive)
        {
            // Update warna realtime (agar slider ImGui bekerja)
            m_cursor->SetColor(m_cursorColor.x, m_cursorColor.y, m_cursorColor.z, m_cursorColor.w);
            m_cursor->Render(context, m_primitive.get());
        }
    }

    // Restore State
    context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    context->RSSetViewports(1, &prevVP);
    context->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
}

void TerminalMonitor1::DrawGUI()
{
    if (ImGui::CollapsingHeader("Monitor 1 (Big Cursor)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (m_srv)
        {
            ImGui::Text("Live Feed:");
            ImGui::Image((void*)m_srv.Get(), ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1));
        }

        ImGui::Separator();
        ImGui::TextColored({ 1, 1, 0, 1 }, "LOCK ANIMATION EDITOR");

        // 1. Checkbox Force Show (Hanya Render)
        ImGui::Checkbox("DEBUG: Force Visibility", &m_debugShowLock);

        // 2. Checkbox Manual Control (Ambil alih Timer)
        ImGui::Checkbox("DEBUG: Manual Scrubbing", &m_manualDebugControl);

        if (m_manualDebugControl)
        {
            ImGui::Indent();
            ImGui::SliderFloat("Scrubber (0-1)", &m_debugAnimProgress, 0.0f, 1.0f);
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Text("Global Settings");
        ImGui::DragFloat2("Center Pos", &m_lockPos.x, 1.0f);
        ImGui::DragFloat("Duration (Sec)", &m_lockAnimDuration, 0.01f, 0.1f, 5.0f);
        ImGui::ColorEdit4("Tint Color", &m_lockColor.x);

        ImGui::Spacing();
        ImGui::Text("Scale Settings (Separate)");
        // [BARU] Slider terpisah
        ImGui::DragFloat("Head Scale", &m_lockHeadScale, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Body Scale", &m_lockBodyScale, 0.01f, 0.1f, 5.0f);

        ImGui::Spacing();
        ImGui::Text("Animation Offsets");
        ImGui::Columns(2, "AnimCols", false);

        ImGui::Text("HEAD (Top)");
        ImGui::DragFloat("Head Start Y", &m_headStartOffsetY, 1.0f);
        ImGui::DragFloat("Head End Y", &m_headEndOffsetY, 1.0f);

        ImGui::NextColumn();

        ImGui::Text("BODY (Bottom)");
        ImGui::DragFloat("Body Start Y", &m_bodyStartOffsetY, 1.0f);
        ImGui::DragFloat("Body End Y", &m_bodyEndOffsetY, 1.0f);

        ImGui::Columns(1);

        ImGui::Spacing();

        // Tombol Test Play sekarang akan bekerja
        if (ImGui::Button("Test Play Animation", ImVec2(-1, 40)))
        {
            ShowLockScreen();
        }

        ImGui::Separator();

        ImGui::Text("Layout Config");
        ImGui::DragFloat("Char Width", &m_charWidth, 1.0f);
        ImGui::DragFloat("Line Spacing", &m_lineSpacing, 1.0f);
        ImGui::DragFloat("Cursor Offset X", &m_cursorOffsetX, 1.0f);
        ImGui::DragFloat("Cursor Base W", &m_cursorBaseW, 1.0f);
        ImGui::DragFloat("Cursor Base H", &m_cursorBaseH, 1.0f);

        ImGui::Text("State: %d", (int)m_animState);
    }
}