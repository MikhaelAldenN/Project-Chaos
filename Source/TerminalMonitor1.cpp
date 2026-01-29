#include "TerminalMonitor1.h"
#include "System/Graphics.h"
#include <imgui.h>
#include <cmath>

TerminalMonitor1::TerminalMonitor1() {}
TerminalMonitor1::~TerminalMonitor1() {}

// =========================================================
// CORE LIFECYCLE
// =========================================================

void TerminalMonitor1::Initialize(ID3D11Device* device, int width, int height)
{
    m_width = width;
    m_height = height;

    // 1. Setup Render Target Texture
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

    // 2. Initialize Components
    m_primitive = std::make_unique<Primitive>(device);
    m_cursor = std::make_unique<CursorBlock>(device);

    m_lockSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Beyond/Sprite_LockBody.png");

    // Setup Cursor: Large Block
    m_cursor->Initialize(m_cursorBaseW, m_cursorBaseH);
    m_cursor->SetBlink(true, 0.8f, 0.5f);
    m_cursor->SetGridSnap(false, 0, 0);

    // Initial Position: Center
    m_cursorPos = { (float)width / 2.0f - 22.5f, (float)height / 2.0f - 40.0f };
    m_lockSize = { (float)width, (float)height };

    // Set Warna Cursor Awal
    m_cursor->SetColor(m_cursorColor.x, m_cursorColor.y, m_cursorColor.z, m_cursorColor.w);
}

void TerminalMonitor1::ShowLockScreen()
{
    m_animState = TerminalAnimState::SYSTEM_LOCK;
    m_currentDisplay = "";
    m_displayLines.clear();
}

void TerminalMonitor1::Update(float dt)
{
    float centerX = (m_width / 2.0f) - 22.5f;
    float centerY = (m_height / 2.0f) - 40.0f;

    switch (m_animState)
    {
    case TerminalAnimState::IDLE:
        // Smoothly return to center
        m_cursorPos.x += (centerX - m_cursorPos.x) * 5.0f * dt;
        m_cursorPos.y += (centerY - m_cursorPos.y) * 5.0f * dt;
        break;

    case TerminalAnimState::MOVING_TO_POS:
    {
        // Fast Lerp to Top-Left typing position
        float speed = 10.0f;
        float distX = m_typingStartPos.x - m_cursorPos.x;
        float distY = m_typingStartPos.y - m_cursorPos.y;

        m_cursorPos.x += distX * speed * dt;
        m_cursorPos.y += distY * speed * dt;

        // Threshold check to start typing
        if (std::abs(distX) < 1.0f && std::abs(distY) < 1.0f)
        {
            m_cursorPos = m_typingStartPos;
            m_animState = TerminalAnimState::TYPING;
            m_animTimer = 0.0f;

            // Rapid blink during typing
            m_cursor->SetBlink(true, 0.1f, 0.05f);
        }
    }
    break;

    case TerminalAnimState::TYPING:
        m_animTimer += dt;
        if (m_animTimer >= m_typeSpeed)
        {
            m_animTimer = 0.0f;
            if (m_currentDisplay.length() < m_targetCommand.length())
            {
                char nextChar = m_targetCommand[m_currentDisplay.length()];
                m_currentDisplay += nextChar;

                if (m_displayLines.empty() || m_displayLines.back().length() >= m_maxCharsPerLine)
                {
                    m_displayLines.push_back(std::string(1, nextChar));
                }
                else
                {
                    m_displayLines.back() += nextChar;
                }

                int row = (int)m_displayLines.size() - 1;
                int col = (int)m_displayLines.back().length();

                m_cursorPos.x = m_typingStartPos.x + (col * m_charWidth) + m_cursorOffsetX;
                m_cursorPos.y = m_typingStartPos.y + (row * m_lineSpacing);
            }
            else
            {
                // Typing complete
                m_animState = TerminalAnimState::DONE;
                m_animTimer = 0.0f;

                // Normal blink rate
                m_cursor->SetBlink(true, 0.5f, 0.5f);
            }
        }
        break;

    case TerminalAnimState::DONE:
        // Waiting for external reset
        break;

    case TerminalAnimState::SYSTEM_LOCK:
        // Tidak ada logika kursor khusus, gambar statis.
        // Bisa tambahkan efek glitch atau pulse di sini nanti jika mau.
        break;
    }

    float targetScale = (m_animState == TerminalAnimState::TYPING) ? 0.8f : 1.0f;
    if (m_cursor)
    {
        m_cursor->SetSize(
            m_cursorBaseW * targetScale,
            m_cursorBaseH * targetScale
        );
    }

    UpdateCursorLogic(dt);
}

void TerminalMonitor1::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
{
    if (!font) return;

    // 1. Setup Render Target
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    D3D11_VIEWPORT prevVP;
    UINT numVP = 1;

    context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    context->RSGetViewports(&numVP, &prevVP);

    ID3D11RenderTargetView* rtv = m_rtv.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->RSSetViewports(1, &m_viewport);

    // Clear Background (Transparent/Black)
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(rtv, clearColor);

    // 2. Setup Render State (No Depth + Alpha Blend)
    auto rs = Graphics::Instance().GetRenderState();
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    context->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    // 3. Render Text (Only when typing or done)
    if (m_animState == TerminalAnimState::SYSTEM_LOCK)
    {
        // --- 1. RENDER SPRITE LOCK SCREEN ---
        if (m_lockSprite)
        {
            // Gunakan Variabel Member (m_lockPos, m_lockSize, m_lockColor)
            m_lockSprite->Render(context,
                m_lockPos.x, m_lockPos.y, 0.0f,
                m_lockSize.x, m_lockSize.y,
                0.0f,
                m_lockColor.x, m_lockColor.y, m_lockColor.z, m_lockColor.w);
        }
    }
    else
    {
        // --- 2. RENDER NORMAL (Teks & Kursor) ---
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
            m_cursor->SetColor(m_cursorColor.x, m_cursorColor.y, m_cursorColor.z, m_cursorColor.w);
            m_cursor->Render(context, m_primitive.get());
        }
    }

    // 4. Render Cursor
    if (m_cursor && m_primitive)
    {
        m_cursor->Render(context, m_primitive.get());
    }

    // 5. Restore State
    context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    context->RSSetViewports(1, &prevVP);
    context->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);

    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
}

// =========================================================
// HELPER METHODS
// =========================================================

void TerminalMonitor1::UpdateCursorLogic(float dt)
{
    if (m_cursor)
    {
        m_cursor->Update(dt, m_cursorPos.x, m_cursorPos.y);
    }
}

void TerminalMonitor1::PlayCommandAnimation(const std::string& command)
{
    m_targetCommand = command;
    m_currentDisplay = "";
    m_displayLines.clear();
    m_animState = TerminalAnimState::MOVING_TO_POS;
    m_animTimer = 0.0f;

    // Force visible blink for movement visibility
    m_cursor->SetBlink(true, 99.0f, 0.0f);
}

void TerminalMonitor1::ResetToIdle()
{
    m_animState = TerminalAnimState::IDLE;
    m_targetCommand = "";
    m_currentDisplay = "";
    m_displayLines.clear();
    m_animTimer = 0.0f;

    // Reset blink to standby mode
    if (m_cursor)
    {
        m_cursor->SetBlink(true, 0.8f, 0.5f);
    }
}

bool TerminalMonitor1::IsBusy() const
{
    return m_animState != TerminalAnimState::IDLE && m_animState != TerminalAnimState::DONE;
}

// =========================================================
// DEBUG GUI
// =========================================================

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
        ImGui::Text("Layout Config");
        ImGui::DragFloat("Char Width", &m_charWidth, 1.0f);
        ImGui::DragFloat("Line Spacing", &m_lineSpacing, 1.0f);
        ImGui::DragFloat("Cursor Offset X", &m_cursorOffsetX, 1.0f);
        ImGui::DragInt("Max Chars/Line", &m_maxCharsPerLine, 1, 1, 20);

        ImGui::Separator();
        ImGui::Text("LOCK SCREEN SPRITE");

        // [BARU] Kontrol Posisi, Ukuran, dan Warna Sprite
        ImGui::DragFloat2("Sprite Pos", &m_lockPos.x, 1.0f);
        ImGui::DragFloat2("Sprite Size", &m_lockSize.x, 1.0f);
        ImGui::ColorEdit4("Sprite Color", &m_lockColor.x);

        if (ImGui::Button("Reset Sprite to Fullscreen"))
        {
            m_lockPos = { 0.0f, 0.0f };
            m_lockSize = { (float)m_width, (float)m_height };
            m_lockColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        }

        ImGui::Separator();
        ImGui::Text("State: %d", (int)m_animState);
        ImGui::Text("Buffer: %s", m_currentDisplay.c_str());
    }
}