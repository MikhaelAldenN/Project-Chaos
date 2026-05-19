#include "UIDialogueBox.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include <windows.h> // Untuk GetSystemMetrics

UIDialogueBox::UIDialogueBox() {}

void UIDialogueBox::Initialize()
{
    auto device = Graphics::Instance().GetDevice();

    // Load aset yang diminta
    m_panelSprite = std::make_unique<Sprite>(device, "Data/Sprite/UI/Sprite_DialogueBox.png");
    m_font = std::make_unique<BitmapFont>("Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");
}

void UIDialogueBox::StartDialogue(const std::vector<std::string>& dialogues)
{
    // Bersihkan antrean lama (jika ada)
    std::queue<std::string> empty;
    std::swap(m_dialogueQueue, empty);

    for (const auto& d : dialogues) {
        m_dialogueQueue.push(d);
    }

    if (!m_dialogueQueue.empty()) {
        AdvanceDialogue();
    }
}

void UIDialogueBox::AdvanceDialogue()
{
    if (m_dialogueQueue.empty()) {
        m_state = State::Hidden;
        return;
    }

    m_currentLine = m_dialogueQueue.front();
    m_dialogueQueue.pop();

    m_displayedText = "";
    m_charIndex = 0;
    m_typeTimer = 0.0f;
    m_state = State::Typing;
}

void UIDialogueBox::Update(float dt)
{
    if (m_state == State::Hidden) return;

    // Mengambil input dari singleton Input dan mengecek trigger tombol Spasi
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

        // Fitur Skip: Jika pemain menekan Spasi saat teks sedang mengetik
        if (isConfirmPressed) {
            m_displayedText = m_currentLine;
            m_charIndex = static_cast<int>(m_currentLine.length());
            m_state = State::WaitingForInput;
        }
    }
    else if (m_state == State::WaitingForInput)
    {
        // Lanjut ke dialog berikutnya jika Spasi ditekan
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

    float panelW = 1200.0f; // Bisa disesuaikan dengan ukuran desain UI-mu
    float panelH = 200.0f;
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