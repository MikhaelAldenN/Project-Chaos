#include "LogConsole.h"
#include "TextDatabase.h"
#include <cstdlib> // Untuk rand()

LogConsole::LogConsole()
{
    // Seed random sederhana (ideally di main/framework)
    // srand(time(NULL)); 
}

void LogConsole::Initialize(float x, float y, int maxLines)
{
    m_x = x;
    m_y = y;
    m_maxLines = maxLines;
    m_activeLogs.clear();

    // Isi awal dengan beberapa baris kosong atau intro
    AddLog("--- SYSTEM BOOT ---");
}

void LogConsole::Update(float dt)
{
    m_timer += dt;

    if (m_timer >= m_nextDelay)
    {
        // 1. Reset timer
        m_timer = 0.0f;

        // 2. Tentukan delay berikutnya secara RANDOM
        // Ini kuncinya: kadang cepat (0.05s), kadang mikir dulu (0.5s)
        float randomVal = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        m_nextDelay = 0.05f + (randomVal * 0.4f); // Delay antara 0.05 - 0.45 detik

        // 3. Tambahkan log baru
        AddLog(GetRandomLog());
    }
}

void LogConsole::AddLog(const std::string& message)
{
    m_activeLogs.push_back(message);

    // Jika melebihi batas layar, hapus yang paling atas (Scrolling effect)
    if (m_activeLogs.size() > m_maxLines)
    {
        m_activeLogs.pop_front();
    }
}

std::string LogConsole::GetRandomLog()
{
    const auto& logs = TextDatabase::Instance().GetSystemLogs();
    if (logs.empty()) return "No logs found...";

    int idx = rand() % logs.size();

    // Tambahkan prefix waktu/hex biar keren
    // Contoh: "[0x4A] Allocating memory..."
    char buffer[64];
    int hexCode = rand() % 999;
    sprintf_s(buffer, "[0x%03X] ", hexCode);

    return std::string(buffer) + logs[idx];
}

void LogConsole::SetStyle(float scale, float lineSpacing, float r, float g, float b, float a)
{
    m_scale = scale;
    m_lineSpacing = lineSpacing;
    m_color[0] = r;
    m_color[1] = g;
    m_color[2] = b;
    m_color[3] = a;
}

void LogConsole::Render(BitmapFont* font)
{
    if (!font) return;

    float currentY = m_y;

    // Render dari atas ke bawah
    for (const auto& log : m_activeLogs)
    {
        // Trik Visual: Log paling bawah (terbaru) lebih terang, yang lama memudar
        // (Optional, kalau mau simpel pakai m_color saja)

        font->Draw(log.c_str(), m_x, currentY, m_scale,
            m_color[0], m_color[1], m_color[2], m_color[3]);

        currentY += m_lineSpacing;
    }
}