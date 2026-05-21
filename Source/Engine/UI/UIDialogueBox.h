#pragma once

#include <string>
#include <vector>
#include <memory>
#include <d3d11.h>

#include "System/Sprite.h"
#include "BitmapFont.h"
#include "FontTTF.h"

class UIDialogueBox
{
public:
    enum class State {
        Hidden,
        Typing,
        WaitingForInput
    };

    UIDialogueBox();
    ~UIDialogueBox() = default;

    void Initialize();
    void StartDialogue(const std::vector<std::string>& dialogues);

    void Update(float dt);
    void Render(ID3D11DeviceContext* dc);

    // Render ke window tracking — koordinat relatif ke ukuran window itu sendiri
    void RenderToWindow(ID3D11DeviceContext* dc, float windowW, float windowH);

    bool IsActive() const { return m_state != State::Hidden; }

    int GetCurrentDialogueIndex() const { return m_currentIndex; }

    void SetShowBackground(bool show) { m_showBackground = show; }
    void SetPosition(float x, float y) { m_posX = x; m_posY = y; m_useCustomPos = true; }

    void SetWorldPosition(DirectX::XMFLOAT3 pos) { m_worldPos = pos; m_is3D = true; }
    void Render3D(ID3D11DeviceContext* dc, Camera* camera);

    // strict = true  → wajib tunggu timer, input skip diabaikan
    // strict = false → timer ATAU tombol skip (default lama)
    void SetAutoAdvance(bool enable, float delay = 1.5f, bool strict = false) {
        m_autoAdvance = enable;
        m_autoAdvanceDelay = delay;
        m_strictAutoAdvance = strict;
    }

private:
    void AdvanceDialogue();

private:
    std::unique_ptr<Sprite>     m_panelSprite;
    //std::unique_ptr<BitmapFont> m_font;
    std::unique_ptr<FontTTF> m_font;

    State m_state = State::Hidden;

    // [REFACTOR] Menggunakan Vector + Index agar lebih scalable
    std::vector<std::string> m_dialogues;
    int m_currentIndex = -1;

    std::string m_currentLine;
    std::string m_displayedText;

    int   m_charIndex = 0;
    float m_typeTimer = 0.0f;
    float m_typeDelay = 0.05f; // Kecepatan efek mesin tik

    bool m_showBackground = true;

    bool  m_useCustomPos = false;
    float m_posX = 0.0f;
    float m_posY = 0.0f;

    bool m_is3D = false;
    DirectX::XMFLOAT3 m_worldPos = { 0.0f, 0.0f, 0.0f };

    // Auto-advance
    bool  m_autoAdvance = false;
    float m_autoAdvanceTimer = 0.0f;
    float m_autoAdvanceDelay = 1.5f;
    bool  m_strictAutoAdvance = false; // true = player wajib tunggu timer, tidak bisa skip
};