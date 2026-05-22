#include "UIDialogueBox.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include <windows.h>

UIDialogueBox::UIDialogueBox() {}

void UIDialogueBox::Initialize()
{
    auto device = Graphics::Instance().GetDevice();
    m_panelSprite = std::make_unique<Sprite>(device, "Data/Sprite/UI/Sprite_DialogueBox.png");

    // Semua karakter yang dipakai di dialog Jepang (kana + kanji + tanda baca)
    // Di-generate dari: "WASDで移動" / "カーソルで狙い、スペースで射撃" / "シフトでダッシュ" / "ウィンドウを撃て"
    std::vector<uint32_t> requiredKanji = {
        0x3001, // 、
        0x3044, // い
        0x3066, // て
        0x3067, // で
        0x3092, // を
        0x30A3, // ィ
        0x30A6, // ウ
        0x30AB, // カ
        0x30B7, // シ
        0x30B9, // ス
        0x30BD, // ソ
        0x30C0, // ダ
        0x30C3, // ッ
        0x30C8, // ト
        0x30C9, // ド
        0x30D5, // フ
        0x30DA, // ペ
        0x30E5, // ュ
        0x30EB, // ル
        0x30F3, // ン
        0x30FC, // ー
        0x52D5, // 動
        0x5C04, // 射
        0x6483, // 撃
        0x72D9, // 狙
        0x79FB, // 移
    };

    m_font = std::make_unique<FontTTF>();
    // Inisialisasi file font ttf langsung dengan ukuran pixel tajam (misal 24px atau 32px)
    m_font->Initialize("Data/Font/zpix.ttf", 28.0f, requiredKanji);
    m_font->Initialize("Data/Font/PixelMplus10-Regular.ttf", 32.0f, requiredKanji);
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
    m_autoAdvanceTimer = 0.0f;

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

    // Jika auto-advance aktif, input player SELALU diabaikan sepenuhnya
    // (baik strict maupun non-strict — auto-advance berarti sistem yang kontrol)
    //bool isConfirmPressed = false;
    //if (!m_autoAdvance)
    //    isConfirmPressed = Input::Instance().GetKeyboard().IsTriggered(VK_SPACE);
    bool isConfirmPressed = Input::Instance().GetKeyboard().IsTriggered(VK_RETURN);

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
                else if ((c & 0xF0) == 0xE0) charLength = 3;
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

        // Skip animasi mesin tik hanya jika BUKAN auto-advance
        if (isConfirmPressed) {
            m_displayedText = m_currentLine;
            m_charIndex = static_cast<int>(m_currentLine.length());
            m_state = State::WaitingForInput;
        }
    }
    else if (m_state == State::WaitingForInput)
    {
        if (m_autoAdvance) {
            m_autoAdvanceTimer += dt;
            if (m_autoAdvanceTimer >= m_autoAdvanceDelay) {
                m_autoAdvanceTimer = 0.0f;
                AdvanceDialogue();
            }
        }
        else {
            if (isConfirmPressed) {
                AdvanceDialogue();
            }
        }
    }
}

void UIDialogueBox::Render(ID3D11DeviceContext* dc)
{
    // [UBAH] Jangan return jika !m_panelSprite, karena kita mungkin hanya butuh font-nya saja
    if (m_state == State::Hidden || !m_font) return;

    // Set Blending untuk 2D UI agar transparan
    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // Hitung posisi Panel UI (Di tengah-bawah layar)
    float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    float panelW = 847.0f; // Bisa disesuaikan dengan ukuran desain UI-mu
    float panelH = 198.0f;
    float panelX = m_useCustomPos ? m_posX : (screenW - panelW) * 0.5f;
    float panelY = m_useCustomPos ? m_posY : (screenH - panelH - 60.0f);

    // [UBAH] Render Panel Background hanya jika flag disetel ke true
    if (m_showBackground && m_panelSprite) {
        m_panelSprite->Render(dc,
            panelX, panelY, 0.0f,     // dx, dy, dz
            panelW, panelH,           // dw, dh
            0.0f,                     // angle
            1.0f, 1.0f, 1.0f, 1.0f    // r, g, b, a
        );
    }

    // Render Teks (Offset sedikit dari pojok panel maya)
    float textMarginX = 40.0f;
    float textMarginY = 40.0f;

    m_font->Draw(m_displayedText, panelX + textMarginX, panelY + textMarginY, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void UIDialogueBox::Render3D(ID3D11DeviceContext* dc, Camera* camera)
{
    if (m_state == State::Hidden || !m_font || !camera) return;

    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // Render Teks di dunia 3D. 
    // Perhatikan scale-nya! Di 3D kita pakai nilai kecil (cth: 0.05f) karena ini satuan meter, bukan piksel.
    m_font->Draw3D(m_displayedText, camera, m_worldPos, 0.05f, { 1.0f, 1.0f, 1.0f, 1.0f });
}

// ---------------------------------------------------------------
// RenderToWindow
// Dipanggil saat engine merender kamera milik tracking window
// dialogue. Koordinat dihitung dari ukuran window itu sendiri
// (windowW x windowH), bukan dari ukuran layar penuh.
// ---------------------------------------------------------------
void UIDialogueBox::RenderToWindow(ID3D11DeviceContext* dc, float windowW, float windowH)
{
    if (m_state == State::Hidden || !m_font) return;

    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // Background panel opsional (biasanya false untuk window solid ini,
    // karena background sudah dari warna window OS-nya sendiri)
    if (m_showBackground && m_panelSprite) {
        m_panelSprite->Render(dc,
            0.0f, 0.0f, 0.0f,
            windowW, windowH,
            0.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        );
    }

    // Teks dengan margin dari tepi client area window
    const float marginX = 14.0f;
    const float marginY = 30.0f;
    m_font->Draw(m_displayedText, marginX, marginY, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
}