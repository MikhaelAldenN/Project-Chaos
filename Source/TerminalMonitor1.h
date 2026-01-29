// TerminalMonitor1.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <deque>
#include <DirectXMath.h>
#include "BitmapFont.h"
#include "CursorBlock.h" // Include CursorBlock
#include "Primitive.h"   // Include Primitive

enum class TerminalAnimState
{
    IDLE,           // Cursor diam di tengah
    MOVING_TO_POS,  // Cursor jalan dari tengah ke kiri atas
    TYPING,         // Ngetik huruf satu-satu
    DONE            // Selesai ngetik (tanda bagi Boss State untuk lanjut)
};

class TerminalMonitor1
{
public:
    TerminalMonitor1();
    ~TerminalMonitor1();

    void Initialize(ID3D11Device* device, int width, int height);
    void Update(float dt);
    void RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font);
    void PlayCommandAnimation(const std::string& command); // Mulai animasi
    bool IsBusy() const; // Cek apakah animasi masih jalan?
    
    ID3D11ShaderResourceView* GetTexture() const { return m_srv.Get(); }

    void AddLog(const std::string& msg);
    void Clear();
    void DrawGUI();
    void MarkAsDirty() { m_needsUpdate = true; }

private:
    void InitializeLogs(); // Helper data dummy
    void UpdateCursorPosition(float dt);

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
    size_t m_maxLines = 14; // Bisa berbeda dari monitor 2
    bool m_needsUpdate = true;

    // --- AUTO SCROLL SYSTEM ---
    std::vector<std::string> m_bootSequence;
    float m_timer = 0.0f;
    float m_lineDelay = 0.4f; // Kecepatan berbeda dari monitor 2
    bool m_autoScrollActive = true;

    // --- VISUAL SETTINGS ---
    // Warna hijau klasik terminal (berbeda dari monitor 2)
    DirectX::XMFLOAT4 m_textColor = { 1.0f, 0.75f, 0.0f, 1.0f };

    // Posisi awal teks (Padding kiri & atas)
    DirectX::XMFLOAT2 m_textPadding = { 20.0f, 20.0f };

    // Jarak antar baris
    float m_lineSpacing = 40.0f;

    // Ukuran Font Global
    float m_fontScale = 0.80f;

    std::unique_ptr<Primitive> m_primitive;
    std::unique_ptr<CursorBlock> m_cursor;

    // --- [BARU] SYSTEM ANIMASI ---
    TerminalAnimState m_animState = TerminalAnimState::IDLE;

    std::string m_targetCommand = "";   // Teks penuh: "INITIATE LOCK"
    std::string m_currentDisplay = "";  // Teks saat ini: "INITI..."

    DirectX::XMFLOAT2 m_cursorPos = { 0,0 }; // Posisi dinamis cursor
    DirectX::XMFLOAT2 m_startTypingPos = { 50.0f, 100.0f }; // Posisi mulai ngetik (kiri atas)

    float m_animTimer = 0.0f;
    float m_typeSpeed = 0.05f; // Kecepatan ngetik per huruf

    // Visual Settings
    float m_commandFontScale = 2.0f; // Tulisan perintah harus besar
};