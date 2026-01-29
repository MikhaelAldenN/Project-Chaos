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

    // Isi database kata-kata
    InitializeLogs();

    // [BARU] Setup Primitive & Cursor
    m_primitive = std::make_unique<Primitive>(device);

    m_cursor = std::make_unique<CursorBlock>(device);
    // Ukuran kursor disesuaikan dengan ukuran Font Scale terminal (0.75f)
    // Asumsi font base size sekitar 10x20 pixel.
    m_cursor->Initialize(12.0f, 24.0f);
    m_cursor->SetBlink(true, 0.6f, 0.4f);
    m_cursor->SetGridSnap(false, 0, 0); // Di terminal 3D mending smooth atau snap manual
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

void TerminalMonitor1::Update(float dt)
{
    // --- 1. LOGIKA AUTO SCROLL (Gabungan dari fungsi pertama) ---
    if (m_autoScrollActive)
    {
        m_timer += dt;
        if (m_timer > m_lineDelay)
        {
            m_timer = 0.0f;
            if (!m_bootSequence.empty())
            {
                // Ambil log acak
                int index = rand() % m_bootSequence.size();
                std::string log = m_bootSequence[index];

                // Variasi format output
                if (rand() % 4 == 0)
                {
                    log += " [" + std::to_string(rand() % 100) + "%]";
                }
                AddLog(log);
            }
        }
    }

    // --- 2. LOGIKA UPDATE CURSOR (Gabungan dari fungsi kedua) ---
    // PENTING: Pastikan Anda juga sudah mengubah definisi UpdateCursorPosition
    // di .h dan .cpp untuk menerima parameter (float dt) seperti saran sebelumnya.
    UpdateCursorPosition(dt);
}

// 2. Update logic posisi kursor
void TerminalMonitor1::UpdateCursorPosition(float dt) 
{
    if (m_logs.empty()) return;

    // Hitung posisi baris terakhir
    // Rumus: PaddingY + (IndexTerakhir * LineSpacing * FontScale)
    float lastLineIndex = (float)(m_logs.size() - 1);

    float cursorY = m_textPadding.y + (lastLineIndex * m_lineSpacing * m_fontScale);

    // Hitung panjang teks baris terakhir (Perkiraan kasar width per char)
    // Untuk hasil akurat, BitmapFont butuh fungsi 'GetTextWidth', 
    // tapi untuk monospaced/retro, estimasi karakter * lebar cukup.
    std::string lastLog = m_logs.back();
    float charWidth = 14.0f * m_fontScale; // Estimasi lebar per huruf
    float cursorX = m_textPadding.x + (lastLog.length() * charWidth) + 5.0f; // +5px padding

    // Update kursor dengan koordinat virtual ini
    // dt kita ambil 0.016f (60fps) atau simpan dt di class member jika mau presisi
    m_cursor->Update(dt, cursorX, cursorY);
}

void TerminalMonitor1::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
{
    // Optimasi: Skip jika tidak ada perubahan
    if (!m_needsUpdate || !font) return;

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

    // --- GAMBAR TEKS DENGAN CUSTOM SETTINGS ---
    float startX = m_textPadding.x;
    float startY = m_textPadding.y;

    for (const auto& line : m_logs)
    {
        font->Draw(
            line.c_str(),
            startX, startY,
            m_fontScale,
            m_textColor.x,  // R
            m_textColor.y,  // G
            m_textColor.z,  // B
            m_textColor.w   // A
        );

        startY += m_lineSpacing * m_fontScale;
    }

    // [BARU] Render Cursor SETELAH teks, tapi SEBELUM restore RTV
    if (m_cursor && m_primitive)
    {
        // CursorBlock menggunakan BlendState INVERT.
        // Karena background texture terminal transparan/hitam:
        // - Hitam (0,0,0) di-invert jadi Putih.
        // - Teks Hijau di-invert jadi Magenta/Ungu.
        // Ini efek retro yang keren!
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
    if (ImGui::CollapsingHeader("Monitor 1 Terminal", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Preview
        if (m_srv)
        {
            ImGui::Text("Live Feed (Monitor 1):");
            ImGui::Image((void*)m_srv.Get(), ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 1, 0, 1));
        }
        ImGui::Separator();

        // --- AUTO SCROLL CONTROL ---
        ImGui::Text("System Settings");
        ImGui::Checkbox("Auto Scroll##mon1", &m_autoScrollActive);
        ImGui::SliderFloat("Speed##mon1", &m_lineDelay, 0.01f, 0.5f);

        // --- VISUAL CUSTOMIZATION ---
        ImGui::Separator();
        ImGui::Text("Visual Appearance");

        if (ImGui::ColorEdit4("Text Color##mon1", &m_textColor.x)) MarkAsDirty();
        if (ImGui::DragFloat2("Padding##mon1", &m_textPadding.x, 1.0f)) MarkAsDirty();
        if (ImGui::DragFloat("Line Spacing##mon1", &m_lineSpacing, 0.5f, 10.0f, 100.0f)) MarkAsDirty();
        if (ImGui::DragFloat("Font Scale##mon1", &m_fontScale, 0.01f, 0.1f, 5.0f)) MarkAsDirty();

        ImGui::Separator();

        // Manual Input
        static char buffer[128] = "";
        if (ImGui::InputText("Log Input##mon1", buffer, IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            AddLog(buffer);
            sprintf_s(buffer, "");
        }
        ImGui::SameLine();
        if (ImGui::Button("SEND##mon1")) { AddLog(buffer); sprintf_s(buffer, ""); }
        if (ImGui::Button("Clear##mon1")) Clear();
    }
}