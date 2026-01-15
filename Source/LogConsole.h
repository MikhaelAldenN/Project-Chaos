#pragma once

#include <vector>
#include <string>
#include <deque>
#include "BitmapFont.h" // Atau header font kamu

class LogConsole
{
public:
    LogConsole();
    ~LogConsole() = default;

    // Setup posisi dan area
    void Initialize(float x, float y, int maxLines);

    // Logika timer untuk nambah text
    void Update(float dt);

    // Render text yang sedang aktif
    void Render(BitmapFont* font);

    // Bisa dipanggil manual jika ingin inject pesan khusus (misal: "ERROR!")
    void AddLog(const std::string& message);

    void SetStyle(float scale, float lineSpacing, float r, float g, float b, float a);
private:
    // Layout
    float m_x = 0;
    float m_y = 0;
    float m_lineSpacing = 20.0f;
    float m_scale = 0.5f;
    float m_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // Abu-abu biar tidak distract

    // Logic
    std::deque<std::string> m_activeLogs; // List log yang tampil di layar
    int m_maxLines = 10;

    float m_timer = 0.0f;
    float m_nextDelay = 0.0f;

    // Helper untuk ambil random string
    std::string GetRandomLog();
};