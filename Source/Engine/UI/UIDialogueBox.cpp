#include "UIDialogueBox.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include <windows.h>

UIDialogueBox::UIDialogueBox() {}

void UIDialogueBox::Initialize()
{
    auto device = Graphics::Instance().GetDevice();
    m_panelSprite = std::make_unique<Sprite>(device, "Data/Sprite/UI/Sprite_DialogueBox.png");

    // Daftarkan Unicode Codepoint Kanji Jepang yang kamu ketik di naskah dialogmu
    // Žn = 0x59CB, Ž€ = 0x6B7B
    //std::vector<uint32_t> requiredKanji = { 0x59CB, 0x6B7B };
    std::vector<uint32_t> requiredKanji = { 0x6D88, 0x5F85, 0x76DB };

    m_font = std::make_unique<FontTTF>();
    // Inisialisasi file font ttf langsung dengan ukuran pixel tajam (misal 24px atau 32px)
    m_font->Initialize("Data/Font/zpix.ttf", 28.0f, requiredKanji);
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

                // [FIX] Cek panjang Byte huruf UTF-8 agar mesin tik tidak patah-patah
                unsigned char c = m_currentLine[m_charIndex];
                int charLength = 1;
                if ((c & 0xE0) == 0xC0) charLength = 2;
                else if ((c & 0xF0) == 0xE0) charLength = 3; // Huruf Jepang (Hiragana/Katakana/Kanji) selalu 3 byte
                else if ((c & 0xF8) == 0xF0) charLength = 4;

                // Masukkan seluruh Byte karakter utuh ke layar
                for (int i = 0; i < charLength && m_charIndex < m_currentLine.length(); ++i) {
                    m_displayedText += m_currentLine[m_charIndex];
                    m_charIndex++;
                }
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

    m_font->Draw(m_displayedText, panelX + textMarginX, panelY + textMarginY, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

}