// BossTerminal.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <deque>
#include <DirectXMath.h>
#include "BitmapFont.h"

class BossTerminal
{
public:
    BossTerminal();
    ~BossTerminal();

    void Initialize(ID3D11Device* device, int width, int height);
    void Update(float dt);
    void RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font);
    ID3D11ShaderResourceView* GetTexture() const { return m_srv.Get(); }

    void AddLog(const std::string& msg);
    void Clear();
    void DrawGUI();
    void MarkAsDirty() { m_needsUpdate = true; }

private:
    void InitializeLogs(); // Helper data dummy

private:
    // RTT Resources
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    D3D11_VIEWPORT m_viewport;

    // Terminal State
    int m_width = 0;
    int m_height = 0;
    std::deque<std::string> m_logs;
    size_t m_maxLines = 12;
    bool m_needsUpdate = true;

    // --- AUTO SCROLL SYSTEM ---
    std::vector<std::string> m_bootSequence;
    float m_timer = 0.0f;
    float m_lineDelay = 0.5f;
    bool m_autoScrollActive = true;

    // --- [BARU] VISUAL SETTINGS ---
    // Warna Amber Klasik (R=1.0, G=0.7, B=0.0)
    DirectX::XMFLOAT4 m_textColor = { 1.0f, 0.75f, 0.0f, 1.0f };

    // Posisi awal teks (Padding kiri & atas)
    DirectX::XMFLOAT2 m_textPadding = { 27.0f, 23.0f };

    // Jarak antar baris
    float m_lineSpacing = 44.0f;

    // Ukuran Font Global
    float m_fontScale = 0.8f;
};