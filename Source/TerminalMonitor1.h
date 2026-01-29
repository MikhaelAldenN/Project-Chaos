#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <DirectXMath.h>
#include <memory>

#include "BitmapFont.h"
#include "CursorBlock.h"
#include "Primitive.h"
#include "System/Sprite.h"

enum class TerminalAnimState
{
    IDLE,           // Cursor breathing at center
    MOVING_TO_POS,  // Cursor moving to typing start position
    TYPING,         // Typing animation in progress
    DONE,            // Typing finished, waiting for reset
    SYSTEM_LOCK
};

class TerminalMonitor1
{
public:
    TerminalMonitor1();
    ~TerminalMonitor1();

    // --- Core Lifecycle ---
    void Initialize(ID3D11Device* device, int width, int height);
    void Update(float dt);
    void RenderToTexture(ID3D11DeviceContext* context, BitmapFont* font);

    // --- Command & Animation Control ---
    void PlayCommandAnimation(const std::string& command);
    void ShowLockScreen();
    void ResetToIdle();
    bool IsBusy() const;

    // --- Getters & Debug ---
    ID3D11ShaderResourceView* GetTexture() const { return m_srv.Get(); }
    void DrawGUI(); // ImGui Debugger


private:
    // --- Internal Helpers ---
    void UpdateCursorLogic(float dt);

private:
    // --- D3D Resources (RTT) ---
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    D3D11_VIEWPORT                                   m_viewport;

    // --- Components ---
    std::unique_ptr<Primitive>   m_primitive;
    std::unique_ptr<CursorBlock> m_cursor;
    std::unique_ptr<Sprite>      m_lockSprite;

    // --- Dimensions & Layout ---
    int   m_width = 0;
    int   m_height = 0;
    float m_fontScale = 2.0f;           // Large font for commands
    float m_charWidth = 45.0f;          // Estimated width per character
    float m_lineSpacing = 80.0f;        // Vertical distance between lines
    int   m_maxCharsPerLine = 8;        // Text wrapping limit
    std::vector<std::string> m_displayLines;

    // --- Animation State ---
    TerminalAnimState   m_animState = TerminalAnimState::IDLE;
    std::string         m_targetCommand = "";
    std::string         m_currentDisplay = "";

    // --- Positioning & Timing ---
    DirectX::XMFLOAT2   m_cursorPos = { 0.0f, 0.0f };
    DirectX::XMFLOAT2   m_typingStartPos = { 50.0f, 100.0f };
    float               m_cursorOffsetX = 50.0f; // Offset to prevent cursor overlapping text

    float               m_animTimer = 0.0f;
    float               m_typeSpeed = 0.05f;     // Seconds per character

    float               m_cursorBaseW = 45.0f;
    float               m_cursorBaseH = 80.0f;

    DirectX::XMFLOAT2 m_lockPos = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_lockSize = { 512.0f, 512.0f }; // Asumsi texture size 512
    DirectX::XMFLOAT4 m_lockColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // [BARU] Settings Warna Cursor (Untuk ImGui)
    DirectX::XMFLOAT4 m_cursorColor = { 0.96f, 0.80f, 0.23f, 1.0f };
};