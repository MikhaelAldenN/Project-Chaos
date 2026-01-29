#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <memory>

#include "BitmapFont.h"
#include "CursorBlock.h"
#include "Primitive.h"
#include "System/Sprite.h" 

enum class TerminalAnimState
{
    IDLE,
    MOVING_TO_POS,
    TYPING,
    DONE,
    SYSTEM_LOCK
};

enum class LockPhase
{
    INACTIVE,
    WARNING,    // Teks Warning (3 detik)
    PRE_DELAY,  // Sprite Muncul Diam (1 detik)
    CLOSING,    // Animasi Menutup
    LOCKED,     // Diam Tertutup
    OPENING     // Animasi Membuka
};

class TerminalMonitor1
{
public:
    TerminalMonitor1();
    ~TerminalMonitor1();

    void Initialize(ID3D11Device* device, int width, int height);
    void Update(float dt);
    void RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font);

    void PlayCommandAnimation(const std::string& command);
    void ShowLockScreen();
    void ResetToIdle(bool force = false);
    bool IsBusy() const;

    ID3D11ShaderResourceView* GetTexture() const { return m_srv.Get(); }
    void DrawGUI();

private:
    void UpdateCursorLogic(float dt);

private:
    // D3D Resources
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    D3D11_VIEWPORT                                   m_viewport;

    // Components
    std::unique_ptr<Primitive>   m_primitive;
    std::unique_ptr<CursorBlock> m_cursor;
    std::unique_ptr<Sprite>      m_lockBodySprite; // Ganti nama biar jelas
    std::unique_ptr<Sprite>      m_lockHeadSprite; // Sprite Kepala Gembok

    // --- [UPDATE] SPRITE LOCK SETTINGS ---
    DirectX::XMFLOAT2 m_lockPos = { 257.0f, 274.0f };
    float m_lockHeadScale = 0.3f;
    float m_lockBodyScale = 0.3f;
    DirectX::XMFLOAT4 m_lockColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool              m_debugShowLock = false; // Toggle Debug
    bool m_manualDebugControl = false;

    // [BARU] Variable Animasi Gembok
    float m_lockAnimTimer = 0.0f;
    float m_lockAnimDuration = 0.4f;  // Durasi animasi (detik)
    
    float m_headStartOffsetY = -150.0f; // Mulai dari atas
    float m_headEndOffsetY = 0.0f;    // Berakhir di tengah (0)

    // Body (Bagian Bawah)
    float m_bodyStartOffsetY = 150.0f;  // Mulai dari bawah
    float m_bodyEndOffsetY = 0.0f;    // Berakhir di tengah (0)

    // Variable Dinamis (Dihitung setiap frame)
    float m_currentHeadY = 0.0f;
    float m_currentBodyY = 0.0f;

    // Debugging Tool
    float m_debugAnimProgress = 0.0f; // Slider 0.0 - 1.0 untuk scrub animasi manual
    
    float m_lockStartGap = 150.0f; // Jarak pisah awal (Head di atas, Body di bawah)
    float m_currentGap = 0.0f;   // Jarak saat ini (dihitung di Update)

    // Cursor Settings
    DirectX::XMFLOAT4 m_cursorColor = { 0.96f, 0.80f, 0.23f, 1.0f };

    // Dimensions & Layout
    int   m_width = 0;
    int   m_height = 0;
    float m_fontScale = 2.0f;
    float m_lineSpacing = 80.0f;
    float m_maxCharsPerLine = 8;

    // Cursor Dimensions
    float m_cursorBaseW = 45.0f;
    float m_cursorBaseH = 80.0f;
    float m_currentScale = 1.0f;

    // Animation State
    TerminalAnimState   m_animState = TerminalAnimState::IDLE;
    std::string         m_targetCommand = "";
    std::string         m_currentDisplay = "";
    std::vector<std::string> m_displayLines;

    // Positioning
    DirectX::XMFLOAT2   m_cursorPos = { 0.0f, 0.0f };
    DirectX::XMFLOAT2   m_typingStartPos = { 50.0f, 100.0f };
    float               m_cursorOffsetX = 20.0f;
    float               m_charWidth = 45.0f;

    float               m_animTimer = 0.0f;
    float               m_typeSpeed = 0.05f;

    // --- [UPDATE] LOGIC VARIABEL ---
    LockPhase m_lockPhase = LockPhase::INACTIVE;
    float     m_lockPhaseTimer = 0.0f;

    // Settings Durasi
    float m_durationWarning = 3.0f;
    float m_durationPreDelay = 1.0f;
    float m_durationAnim = 0.4f; // Cepat
};