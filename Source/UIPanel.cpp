#include "UIPanel.h"
#include "System/Input.h" // Pastikan include Input system kamu

UIPanel::UIPanel(Primitive* prim, float x, float y, float w, float h, const std::string& title)
    : primitive(prim), x(x), y(y), width(w), height(h), title(title)
{
}

void UIPanel::SetMessage(const std::string& msg)
{
    // Reset dan pecah pesan sederhana (bisa dikembangkan logic split stringnya)
    messageLines.clear();
    messageLines.push_back(msg);
    // Kalau mau canggih, bisa split by '\n' di sini manual
}

void UIPanel::AddButton(const std::string& label, float relX, float relY, float w, float h, std::function<void()> onClick)
{
    auto btn = std::make_unique<UIButtonPrimitive>(primitive, label, x + relX, y + relY, w, h);
    btn->SetOnClick(onClick);
    btn->SetAlignment(TextAlignment::Center);

    // [BARU] Setup Style Tombol ala DOS (Transparan + Border Kuning)

    // Definisi Warna
    DirectX::XMFLOAT4 transparent = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 yellow = { 0.96f, 0.80f, 0.23f, 1.0f };
    DirectX::XMFLOAT4 yellowHover = { 0.96f, 0.80f, 0.23f, 0.2f }; // Kuning transparan untuk hover
    DirectX::XMFLOAT4 blue = { 0.0f, 0.0f, 0.8f, 1.0f };
    DirectX::XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    // State 1: STANDBY (Normal) -> Background Transparan, Border Kuning, Teks Kuning
    btn->SetStyle(ButtonState::STANDBY, { transparent, transparent, yellow });

    // State 2: HOVER (Kursor di atasnya) -> Background Kuning Tipis, Teks Kuning
    btn->SetStyle(ButtonState::HOVER, { blue, blue, white });

    // State 3: PRESSED (Diklik) -> Background Kuning Solid, Teks Hitam
    btn->SetStyle(ButtonState::PRESSED, { yellow, yellow, {0.0f, 0.0f, 0.0f, 1.0f} });

    buttons.push_back(std::move(btn));
}

void UIPanel::Update(float dt)
{
    if (!isVisible) return;

    // Update semua tombol di dalam panel
    for (auto& btn : buttons)
    {
        btn->Update();
    }
}

void UIPanel::Render(ID3D11DeviceContext* dc)
{
    if (!isVisible) return;

    // --- TAHAP 1: Gambar Kotak Panel ---

    // 1. Background Hitam Solid
    // Format: Rect(x, y, w, h, cx, cy, angle, r, g, b, a)
    primitive->Rect(x - 8, y - 8, width + 16, height + 16,
        0.0f, 0.0f, 0.0f, // [FIX] Tambahkan 3 parameter ini (cx, cy, angle)
        colorBg.x, colorBg.y, colorBg.z, colorBg.w);

    // 2. Border Luar Kuning
    primitive->Rect(x, y, width, height,
        0.0f, 0.0f, 0.0f, // [FIX] Tambahkan 3 parameter ini
        colorBorder.x, colorBorder.y, colorBorder.z, colorBorder.w);

    // 3. Border Dalam Hitam (Untuk menciptakan efek garis border tipis)
    float borderThick = 2.5f;
    primitive->Rect(x + borderThick, y + borderThick, width - (borderThick * 2), height - (borderThick * 2),
        0.0f, 0.0f, 0.0f, // [FIX] Tambahkan 3 parameter ini
        colorBg.x, colorBg.y, colorBg.z, colorBg.w);

    // --- TAHAP 2: Siapkan Area Label Judul ---

    BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
    if (font)
    {
        // Hitung ukuran background label
        DirectX::XMFLOAT2 titleSize = font->MeasureText(title, 0.625f);
        float labelPad = 8.0f;

        // Posisi kotak penimpa
        float blockerX = x + 27.0f - labelPad;
        float blockerY = y - (borderThick * 2.0f);
        float blockerW = titleSize.x + (labelPad * 2.0f);
        float blockerH = borderThick * 4.0f;

        // Gambar kotak hitam penimpa garis border
        primitive->Rect(blockerX, blockerY, blockerW - 10, blockerH,
            0.0f, 0.0f, 0.0f, // [FIX] Tambahkan 3 parameter ini
            colorBg.x, colorBg.y, colorBg.z, colorBg.w);
    }

    // PENTING: Flush semua kotak di atas sekarang.
    primitive->Render(dc);

    // --- TAHAP 3: Gambar Teks & Tombol ---

    if (font)
    {
        // 1. Gambar Teks Judul (Kuning)
        font->Draw(title.c_str(), x + 25.0f, y - 8.0f, 0.625f,
            colorText.x, colorText.y, colorText.z, colorText.w);

        // 2. Gambar Pesan Body
        float msgY = y + 80.0f;
        for (const auto& line : messageLines)
        {
            DirectX::XMFLOAT2 msgSize = font->MeasureText(line, 0.625f);
            float msgX = x + (width - msgSize.x) / 2.0f + 200;

            font->Draw(line.c_str(), msgX, msgY, 0.625f,
                colorText.x, colorText.y, colorText.z, colorText.w);
            msgY += 25.0f;
        }
    }

    // 3. Gambar Tombol
    for (auto& btn : buttons)
    {
        btn->Render(dc, nullptr);
    }
}