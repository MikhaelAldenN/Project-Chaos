#include "UIDialogueBox.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include <windows.h>

UIDialogueBox::UIDialogueBox() {}

void UIDialogueBox::Initialize()
{
    auto device = Graphics::Instance().GetDevice();
    m_panelSprite = std::make_unique<Sprite>(device, "Data/Sprite/UI/Sprite_DialogueBox.png");
    m_font = std::make_unique<BitmapFont>("Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");
}

void UIDialogueBox::StartDialogue(const std::vector<std::string>& dialogues)
{
    m_dialogues = dialogues;
    m_currentIndex = -1; // Akan menjadi 0 saat AdvanceDialogue dipanggil

    if (!m_dialogues.empty()) {
        AdvanceDialogue();
    }
}

void UIDialogueBox::AdvanceDialogue()
{
    m_currentIndex++;

    // Jika indeks sudah melebihi jumlah dialog, sembunyikan
    if (m_currentIndex >= static_cast<int>(m_dialogues.size())) {
        m_state = State::Hidden;
        return;
    }

    m_currentLine = m_dialogues[m_currentIndex];
    m_displayedText = "";
    m_charIndex = 0;
    m_typeTimer = 0.0f;
    m_state = State::Typing;
}

void UIDialogueBox::Update(float dt)
{
    if (m_state == State::Hidden) return;

    bool isConfirmPressed = Input::Instance().GetKeyboard().IsTriggered(VK_SPACE);

    if (m_state == State::Typing)
    {
        m_typeTimer += dt;
        if (m_typeTimer >= m_typeDelay) {
            m_typeTimer = 0.0f;
            if (m_charIndex < m_currentLine.length()) {
                m_displayedText += m_currentLine[m_charIndex];
                m_charIndex++;
            }
            else {
                m_state = State::WaitingForInput;
            }
        }

        if (isConfirmPressed) {
            m_displayedText = m_currentLine;
            m_charIndex = static_cast<int>(m_currentLine.length());
            m_state = State::WaitingForInput;
        }
    }
    else if (m_state == State::WaitingForInput)
    {
        if (isConfirmPressed) {
            AdvanceDialogue();
        }
    }
}

void UIDialogueBox::Render(ID3D11DeviceContext* dc)
{
    if (m_state == State::Hidden || !m_panelSprite || !m_font) return;

    // Set Blending untuk 2D UI agar transparan
    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // Hitung posisi Panel UI (Di tengah-bawah layar)
    float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    float panelW = 847.0f; // Bisa disesuaikan dengan ukuran desain UI-mu
    float panelH = 198.0f;
    float panelX = (screenW - panelW) * 0.5f;
    float panelY = screenH - panelH - 60.0f; // Jarak 60 pixel dari bawah layar

    // Render Panel Background
    m_panelSprite->Render(dc,
        panelX, panelY, 0.0f,     // dx, dy, dz
        panelW, panelH,           // dw, dh
        0.0f,                     // angle
        1.0f, 1.0f, 1.0f, 1.0f    // r, g, b, a
    );

    // Render Teks (Offset sedikit dari pojok panel)
    float textMarginX = 40.0f;
    float textMarginY = 40.0f;
    float textScale = 1.0f;

    m_font->Draw(m_displayedText, panelX + textMarginX, panelY + textMarginY, textScale, 1.0f, 1.0f, 1.0f, 1.0f);
}