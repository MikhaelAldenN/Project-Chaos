#include "BitmapFont.h"
#include "System/Graphics.h" // Untuk mengambil Device & Context
#include <fstream>
#include <sstream>

BitmapFont::BitmapFont(const std::string& texturePath, const std::string& fontDataPath)
{
    // 1. Ambil Device dan buat Sprite
    auto device = Graphics::Instance().GetDevice();
    sprite = std::make_unique<Sprite>(device, texturePath.c_str());

    // 2. Parse file .fnt
    std::ifstream file(fontDataPath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        // Kita hanya ambil baris yang mendefinisikan karakter ("char id=...")
        if (line.find("char id=") != std::string::npos)
        {
            CharData c;
            c.id = ParseValue(line, "id");
            c.x = ParseValue(line, "x");
            c.y = ParseValue(line, "y");
            c.width = ParseValue(line, "width");
            c.height = ParseValue(line, "height");
            c.xoffset = ParseValue(line, "xoffset");
            c.yoffset = ParseValue(line, "yoffset");
            c.xadvance = ParseValue(line, "xadvance");

            chars[c.id] = c;
        }
    }
}

// Fungsi helper untuk mengambil angka dari string seperti "x=10"
int BitmapFont::ParseValue(const std::string& line, const std::string& key)
{
    std::string search = key + "=";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return 0;

    size_t start = pos + search.length();
    size_t end = line.find(" ", start);
    if (end == std::string::npos) end = line.length();

    return std::stoi(line.substr(start, end - start));
}

void BitmapFont::Draw(const std::string& text, float startX, float startY, float scale, float r, float g, float b, float a)
{
    if (!sprite) return;

    // Ambil Context untuk render
    auto dc = Graphics::Instance().GetDeviceContext();

    float cursorX = startX;
    float cursorY = startY;

    // Jarak antar baris (bisa disesuaikan manual atau ambil dari common lineHeight di .fnt)
    float lineHeight = 38.0f * scale;

    for (char c : text)
    {
        if (c == '\n') // Handle Enter/Baris Baru
        {
            cursorX = startX;
            cursorY += lineHeight;
            continue;
        }

        int id = (unsigned char)c;
        // Jika huruf tidak ada di data, lewati
        if (chars.find(id) == chars.end()) continue;

        CharData& data = chars[id];

        // Hitung posisi visual di layar
        float drawX = cursorX + (data.xoffset * scale);
        float drawY = cursorY + (data.yoffset * scale);

        // --- INI BAGIAN PENTINGNYA ---
        // Kita panggil fungsi Render kamu yang support texture slicing (sx, sy, sw, sh)
        sprite->Render(
            dc,
            drawX, drawY, 0.0f,                 // dx, dy, dz (Posisi Layar)
            (float)data.width * scale,          // dw (Lebar di Layar)
            (float)data.height * scale,         // dh (Tinggi di Layar)
            (float)data.x, (float)data.y,       // sx, sy (Posisi Potong di Texture)
            (float)data.width, (float)data.height, // sw, sh (Ukuran Potong di Texture)
            0.0f,                               // Angle
            r, g, b, a                          // Warna
        );

        // Geser kursor X untuk huruf berikutnya
        cursorX += data.xadvance * scale;
    }
}

DirectX::XMFLOAT2 BitmapFont::MeasureText(const std::string& text, float scale)
{
    float width = 0.0f;
    float lineHeight = 38.0f * scale; // Hardcoded sesuai Draw() kamu. Idealnya ini dibaca dari file .fnt ("common lineHeight")

    // Kalau kosong, return 0
    if (text.empty()) return { 0.0f, 0.0f };

    // Hitung Lebar
    for (char c : text)
    {
        int id = (unsigned char)c;
        if (chars.find(id) != chars.end())
        {
            // xadvance adalah jarak kursor berpindah ke huruf selanjutnya
            width += chars[id].xadvance * scale;
        }
    }

    // Untuk tinggi, tombol biasanya cuma 1 baris, jadi kita return tinggi font default
    // Kalau mau support multiline, logicnya harus hitung berapa kali '\n' muncul.
    return { width, lineHeight };
}