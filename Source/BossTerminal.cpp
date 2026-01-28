#include "BossTerminal.h"
#include "System/Graphics.h"
#include <imgui.h>

BossTerminal::BossTerminal() {}
BossTerminal::~BossTerminal() {}

void BossTerminal::Initialize(ID3D11Device* device, int width, int height)
{
    m_width = width;
    m_height = height;

    // 1. Buat Texture Render Target
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

    // 2. Buat Views
    device->CreateRenderTargetView(m_renderTargetTexture.Get(), nullptr, m_rtv.GetAddressOf());
    device->CreateShaderResourceView(m_renderTargetTexture.Get(), nullptr, m_srv.GetAddressOf());

    // 3. Viewport khusus terminal
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = (float)width;
    m_viewport.Height = (float)height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
}

void BossTerminal::Update(float dt)
{
    // Logika scrolling sederhana (opsional, bisa dikembangkan)
    // Saat ini hanya static log
}

void BossTerminal::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
{
    // [OPTIMASI FATAL]
    // Jika tidak ada perubahan teks, JANGAN gambar ulang.
    // Biarkan texture yang lama tetap di memori GPU.
    if (!m_needsUpdate || !font) return;

    // --- Simpan State Lama ---
    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    D3D11_VIEWPORT prevVP;
    UINT numVP = 1;

    context->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    context->RSGetViewports(&numVP, &prevVP);

    // ... (Kode Setup RTV, Viewport, Clear, Blend State SAMA SEPERTI SEBELUMNYA) ...
    // ... Copy paste kode RenderToTexture yang sudah benar (dengan Blend State) di sini ...

    // --- SETUP RENDER TARGET ---
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->RSSetViewports(1, &m_viewport);

    // Clear Transparency
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(rtv, clearColor);

    // Setup States
    auto rs = Graphics::Instance().GetRenderState();
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);

    // --- LOOP GAMBAR TEKS (YANG BERAT INI) ---
    float startY = 10.0f;
    float scale = 0.8f;

    for (const auto& line : m_logs)
    {
        font->Draw(line.c_str(), 10.0f, startY, scale, 0.1f, 1.0f, 0.1f, 1.0f);
        startY += 30.0f * scale;
    }
    // ------------------------------------------

    // Restore State
    context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    context->RSSetViewports(1, &prevVP);
    context->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);

    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();

    // [OPTIMASI] Reset flag. Kita sudah selesai gambar.
    // Frame berikutnya akan langsung 'return' di baris paling atas.
    m_needsUpdate = false;
}

void BossTerminal::AddLog(const std::string& msg)
{
    m_logs.push_back(msg);
    if (m_logs.size() > m_maxLines) m_logs.pop_front();

    // [OPTIMASI] Beritahu sistem bahwa teks berubah
    m_needsUpdate = true;
}

void BossTerminal::Clear()
{
    m_logs.clear();

    // [OPTIMASI]
    m_needsUpdate = true;
}

void BossTerminal::DrawGUI()
{
    if (ImGui::CollapsingHeader("Terminal Controller", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 1. LIVE PREVIEW TEXTURE
        // Kita bisa menampilkan texture hasil render RTT langsung di ImGui!
        if (m_srv)
        {
            ImGui::Text("Live Feed:");
            // Tampilkan texture ukuran 256x256
            ImGui::Image((void*)m_srv.Get(), ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 1, 0, 1));
        }

        ImGui::Separator();

        // 2. INPUT MANUAL
        static char buffer[128] = "";
        bool reclaim_focus = false;

        // Input text dengan flag EnterReturnsTrue
        if (ImGui::InputText("Command Input", buffer, IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            AddLog(buffer);
            sprintf_s(buffer, ""); // Clear setelah enter
            reclaim_focus = true;
        }

        // Auto focus biar enak ngetik
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::SameLine();
        if (ImGui::Button("SEND"))
        {
            AddLog(buffer);
            sprintf_s(buffer, "");
        }

        // 3. UTILITY BUTTONS
        if (ImGui::Button("Clear Screen"))
        {
            Clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Boot Log"))
        {
            AddLog("SYSTEM REBOOT...");
            AddLog("KERNEL PANIC [0x0000]");
            AddLog("RETRYING CONNECTION...");
        }

        // 4. SETTINGS (Opsional)
        // ImGui::SliderFloat("Scroll Speed", &m_scrollSpeed, 0.1f, 5.0f);
    }
}