// TerminalMonitor1.cpp
#include "TerminalMonitor1.h"
#include "System/Graphics.h"
#include <imgui.h>

TerminalMonitor1::TerminalMonitor1() {}
TerminalMonitor1::~TerminalMonitor1() {}

void TerminalMonitor1::Initialize(ID3D11Device* device, int width, int height)
{
    m_width = width;
    m_height = height;

    // 1. Texture Setup
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

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = (float)width;
    m_viewport.Height = (float)height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    //// Isi database kata-kata
    //InitializeLogs();

    // [BARU] Setup Primitive & Cursor
    m_primitive = std::make_unique<Primitive>(device);

    m_cursor = std::make_unique<CursorBlock>(device);
    // Ukuran kursor disesuaikan dengan ukuran Font Scale terminal (0.75f)
    // Asumsi font base size sekitar 10x20 pixel.
    m_cursor->Initialize(45.0f, 80.0f);
    m_cursor->SetBlink(true, 0.8f, 0.5f);
    m_cursorPos = { (float)width / 2.0f - 22.5f, (float)height / 2.0f - 40.0f };
}

void TerminalMonitor1::InitializeLogs()
{
    // Konten berbeda dari monitor 2 - fokus ke sistem monitoring
    m_bootSequence = {
        "",
        "MONITOR-1",
        "STATUS: OK",
        "TEMP: 38C",
        "FAN: 2400RPM",
        "VOLT: 12.1V",
        "RAM: 8192MB",
        "DISK: 512GB",
        "NET: 1Gbps",
        "PING: 12ms",
        "CPU: 45%",
        "GPU: 62%",
        "CACHE: HIT",
        "SWAP: 0MB",
        "UPTIME: 99%",
        "ERR: NONE",
        "LOG: CLEAR",
        "PROC: 24",
        "THR: 48",
        "I/O: FAST",
        "RAID: SYNC",
        "BACKUP: OK"
    };
}

void TerminalMonitor1::PlayCommandAnimation(const std::string& command)
{
    m_targetCommand = command;
    m_currentDisplay = "";
    m_animState = TerminalAnimState::MOVING_TO_POS; // Mulai gerak ke kiri
    m_animTimer = 0.0f;

    // Paksa cursor nyala (biar kelihatan pas gerak)
    m_cursor->SetBlink(true, 99.0f, 0.0f);
}

bool TerminalMonitor1::IsBusy() const
{
    return m_animState != TerminalAnimState::IDLE && m_animState != TerminalAnimState::DONE;
}

void TerminalMonitor1::Update(float dt)
{
    float centerX = (m_width / 2.0f) - 22.5f; // Setengah lebar cursor
    float centerY = (m_height / 2.0f) - 40.0f; // Setengah tinggi cursor

    switch (m_animState)
    {
    case TerminalAnimState::IDLE:
        // Logic Lama: Diam di tengah
        // Lerp pelan balik ke tengah kalau habis dari animasi
        m_cursorPos.x += (centerX - m_cursorPos.x) * 5.0f * dt;
        m_cursorPos.y += (centerY - m_cursorPos.y) * 5.0f * dt;
        break;

    case TerminalAnimState::MOVING_TO_POS:
    {
        // Lerp Cepat ke posisi ngetik (Kiri Atas)
        float speed = 10.0f;
        float distX = m_startTypingPos.x - m_cursorPos.x;
        float distY = m_startTypingPos.y - m_cursorPos.y;

        m_cursorPos.x += distX * speed * dt;
        m_cursorPos.y += distY * speed * dt;

        // Jika sudah dekat, mulai ngetik
        if (abs(distX) < 1.0f && abs(distY) < 1.0f)
        {
            m_cursorPos = m_startTypingPos;
            m_animState = TerminalAnimState::TYPING;
            m_animTimer = 0.0f;
            // Kembalikan blink normal saat ngetik
            m_cursor->SetBlink(true, 0.1f, 0.05f); // Ngetik cepat, blink cepat
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
                // Tambah 1 huruf
                m_currentDisplay += m_targetCommand[m_currentDisplay.length()];

                // Geser cursor ke kanan (Estimasi lebar font 20px * scale 2.0 = 40px)
                m_cursorPos.x += 35.0f;
            }
            else
            {
                // Selesai ngetik, diam sebentar (hold)
                m_animState = TerminalAnimState::DONE;
                m_animTimer = 0.0f;
                m_cursor->SetBlink(true, 0.5f, 0.5f); // Blink normal lagi
            }
        }
        break;

    case TerminalAnimState::DONE:
        // Boss State akan membaca ini, lalu memanggil fungsi lain untuk reset
        // Atau kita biarkan saja DONE sampai Boss meresetnya
        break;
    }

    // Update Visual Cursor
    if (m_cursor)
    {
        m_cursor->Update(dt, m_cursorPos.x, m_cursorPos.y);
    }
}
//// 2. Update logic posisi kursor
//void TerminalMonitor1::UpdateCursorPosition(float dt) 
//{
//    if (m_logs.empty()) return;
//
//    // Hitung posisi baris terakhir
//    // Rumus: PaddingY + (IndexTerakhir * LineSpacing * FontScale)
//    float lastLineIndex = (float)(m_logs.size() - 1);
//
//    float cursorY = m_textPadding.y + (lastLineIndex * m_lineSpacing * m_fontScale);
//
//    // Hitung panjang teks baris terakhir (Perkiraan kasar width per char)
//    // Untuk hasil akurat, BitmapFont butuh fungsi 'GetTextWidth', 
//    // tapi untuk monospaced/retro, estimasi karakter * lebar cukup.
//    std::string lastLog = m_logs.back();
//    float charWidth = 14.0f * m_fontScale; // Estimasi lebar per huruf
//    float cursorX = m_textPadding.x + (lastLog.length() * charWidth) + 5.0f; // +5px padding
//
//    // Update kursor dengan koordinat virtual ini
//    // dt kita ambil 0.016f (60fps) atau simpan dt di class member jika mau presisi
//    m_cursor->Update(dt, cursorX, cursorY);
//}

void TerminalMonitor1::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
{
    // Optimasi: Skip jika tidak ada perubahan
    if (!font) return;


    // Backup State
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    D3D11_VIEWPORT prevVP;
    UINT numVP = 1;
    context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    context->RSGetViewports(&numVP, &prevVP);

    // Setup Target
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->RSSetViewports(1, &m_viewport);

    // Clear (Transparan)
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(rtv, clearColor);

    // Setup Render State (No Depth + Alpha Blend)
    auto rs = Graphics::Instance().GetRenderState();
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);

    //// --- GAMBAR TEKS DENGAN CUSTOM SETTINGS ---
    //float startX = m_textPadding.x;
    //float startY = m_textPadding.y;

    //for (const auto& line : m_logs)
    //{
    //    font->Draw(
    //        line.c_str(),
    //        startX, startY,
    //        m_fontScale,
    //        m_textColor.x,  // R
    //        m_textColor.y,  // G
    //        m_textColor.z,  // B
    //        m_textColor.w   // A
    //    );
    
    //    startY += m_lineSpacing * m_fontScale;
    //}

// 1. RENDER TEKS COMMAND (Hanya jika sedang ngetik/done)
    if (m_animState == TerminalAnimState::TYPING || m_animState == TerminalAnimState::DONE)
    {
        // Warna Putih/Merah Besar
        font->Draw(
            m_currentDisplay.c_str(),
            m_startTypingPos.x, m_startTypingPos.y,
            m_commandFontScale,
            1.0f, 0.2f, 0.2f, 1.0f // Merah Alarm
        );
    }

    // 2. RENDER CURSOR (Selalu di atas teks)
    if (m_cursor && m_primitive)
    {
        m_cursor->Render(context, m_primitive.get());
    }

    // Restore State
    context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    context->RSSetViewports(1, &prevVP);
    context->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);

    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();

    m_needsUpdate = false;
}

void TerminalMonitor1::AddLog(const std::string& msg)
{
    m_logs.push_back(msg);
    if (m_logs.size() > m_maxLines) m_logs.pop_front();
    m_needsUpdate = true;
}

void TerminalMonitor1::Clear()
{
    m_logs.clear();
    m_needsUpdate = true;
}

void TerminalMonitor1::DrawGUI()
{
    // Update GUI sedikit biar ga bingung
    if (ImGui::CollapsingHeader("Monitor 1 (Big Cursor)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (m_srv)
        {
            ImGui::Text("Live Feed:");
            ImGui::Image((void*)m_srv.Get(), ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1));
        }
        ImGui::TextDisabled("Log System Deactivated.");
    }
}