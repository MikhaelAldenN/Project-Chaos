// BossTerminal.cpp
#include "BossTerminal.h"
#include "System/Graphics.h"
#include <imgui.h>

BossTerminal::BossTerminal() {}
BossTerminal::~BossTerminal() {}

void BossTerminal::Initialize(ID3D11Device* device, int width, int height)
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
}

void BossTerminal::InitializeLogs()
{
    // Maksimal panjang base string di sini sekitar 12-14 karakter
    // Agar jika ditambah timer [xx ms] tetap di bawah 22.
    m_bootSequence = {
        "",
        "SYS INIT...",
        "MEM: 0xA4 OK",
        "FWALL: UP",
        "DRIVE: MNT",
        "CHECK: OK",
        "PKT: [||||]",
        "DECRYPT...",
        "AUTH: ADMIN",
        "CPU: 42C",
        "SHADER: LD",
        "DX11: RDY",
        "LINK: SEC",
        "SCAN: CLR",
        "EXE: START",
        "NET: LINK",
        "UPL: 100%",
        "SYNC: OK",
        "THRD: OPT",
        "BUFF: CLR",
        "WDOG: ON",
        "RUN: LOOP"
    };
}
void BossTerminal::Update(float dt)
{
    if (!m_autoScrollActive) return;

    m_timer += dt;
    if (m_timer > m_lineDelay)
    {
        m_timer = 0.0f;
        if (!m_bootSequence.empty())
        {
            int index = rand() % m_bootSequence.size();
            std::string log = m_bootSequence[index];
            if (rand() % 3 == 0) log += " [" + std::to_string(rand() % 999) + " ms]";
            AddLog(log);
        }
    }
}

void BossTerminal::RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font)
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
    // Gunakan variabel Padding sebagai posisi awal
    float startX = m_textPadding.x;
    float startY = m_textPadding.y;

    for (const auto& line : m_logs)
    {
        font->Draw(
            line.c_str(),
            startX, startY,         // Posisi X, Y (Custom Offset)
            m_fontScale,            // Custom Scale
            m_textColor.x,          // R (Custom Color)
            m_textColor.y,          // G
            m_textColor.z,          // B
            m_textColor.w           // A
        );

        // Geser ke bawah sesuai Line Spacing
        startY += m_lineSpacing * m_fontScale;
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

void BossTerminal::AddLog(const std::string& msg)
{
    m_logs.push_back(msg);
    if (m_logs.size() > m_maxLines) m_logs.pop_front();
    m_needsUpdate = true;
}

void BossTerminal::Clear()
{
    m_logs.clear();
    m_needsUpdate = true;
}

void BossTerminal::DrawGUI()
{
    if (ImGui::CollapsingHeader("Terminal Controller", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Preview
        if (m_srv)
        {
            ImGui::Text("Live Feed:");
            ImGui::Image((void*)m_srv.Get(), ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 1, 0, 1));
        }
        ImGui::Separator();

        // --- AUTO SCROLL CONTROL ---
        ImGui::Text("System Settings");
        ImGui::Checkbox("Auto Scroll", &m_autoScrollActive);
        ImGui::SliderFloat("Speed", &m_lineDelay, 0.01f, 0.5f);

        // --- [BARU] VISUAL CUSTOMIZATION ---
        ImGui::Separator();
        ImGui::Text("Visual Appearance");

        // Menggunakan MarkAsDirty() agar setiap perubahan slider langsung terlihat
        if (ImGui::ColorEdit4("Text Color", &m_textColor.x)) MarkAsDirty();
        if (ImGui::DragFloat2("Padding (Offset)", &m_textPadding.x, 1.0f)) MarkAsDirty();
        if (ImGui::DragFloat("Line Spacing", &m_lineSpacing, 0.5f, 10.0f, 100.0f)) MarkAsDirty();
        if (ImGui::DragFloat("Font Scale", &m_fontScale, 0.01f, 0.1f, 5.0f)) MarkAsDirty();

        ImGui::Separator();

        // Manual Input
        static char buffer[128] = "";
        if (ImGui::InputText("Log Input", buffer, IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            AddLog(buffer);
            sprintf_s(buffer, "");
        }
        ImGui::SameLine();
        if (ImGui::Button("SEND")) { AddLog(buffer); sprintf_s(buffer, ""); }
        if (ImGui::Button("Clear")) Clear();
    }
}