#pragma once

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <d3d11.h>

#include "System/Sprite.h"
#include "BitmapFont.h"

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

    // Untuk mengecek apakah Boss Logic harus di-pause
    bool IsActive() const { return m_state != State::Hidden; }

private:
    void AdvanceDialogue();

private:
    std::unique_ptr<Sprite>     m_panelSprite;
    std::unique_ptr<BitmapFont> m_font;

    State m_state = State::Hidden;
    std::queue<std::string> m_dialogueQueue;

    std::string m_currentLine;
    std::string m_displayedText;

    int   m_charIndex = 0;
    float m_typeTimer = 0.0f;
    float m_typeDelay = 0.05f; // Kecepatan efek mesin tik (detik per huruf)
};