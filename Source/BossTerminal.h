#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <deque>
#include <DirectXMath.h>
#include "BitmapFont.h" // Pastikan include BitmapFont kamu

class BossTerminal
{
public:
    BossTerminal();
    ~BossTerminal();

    // Inisialisasi Resolusi Layar Virtual (misal 512x512 cukup tajam untuk monitor kecil)
    void Initialize(ID3D11Device* device, int width, int height);

    // Update logika teks (typewriter, scrolling)
    void Update(float dt);

    // Gambar teks ke dalam Texture (Off-screen rendering)
    // Panggil ini SEBELUM scene utama dirender!
    void RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font);

    // Ambil hasil texture untuk ditempel ke monitor
    ID3D11ShaderResourceView* GetTexture() const { return m_srv.Get(); }

    // Fungsi Logika
    void AddLog(const std::string& msg);
    void Clear();

    void DrawGUI();
    void MarkAsDirty() { m_needsUpdate = true; }
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
    float m_scrollTimer = 0.0f;
    size_t m_maxLines = 15;

    // Typewriter State (Opsional)
    std::string m_currentLineBuffer;
    // ... tambahkan variabel typewriter kamu sendiri
    bool m_needsUpdate = true;
};