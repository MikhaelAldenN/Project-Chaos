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

    bool IsActive() const { return m_state != State::Hidden; }

    // [NEW] Untuk sinkronisasi event di luar class
    int GetCurrentDialogueIndex() const { return m_currentIndex; }

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
};