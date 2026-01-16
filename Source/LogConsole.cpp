#include "LogConsole.h"
#include "TextDatabase.h"
#include <cstdlib>

LogConsole::LogConsole()
{
    // srand(time(NULL)); // Pastikan ini dipanggil sekali di Main.cpp, jangan di sini kalau bisa.
}

void LogConsole::Initialize(float x, float y, int maxLines)
{
    m_x = x;
    m_y = y;
    m_maxLines = maxLines;

    // [OPTIMASI] Siapkan semua slot memori SEKARANG.
    // Jadi nanti saat main game, tidak ada lagi minta-minta memori ke RAM.
    m_buffer.resize(maxLines);
    for (int i = 0; i < maxLines; ++i) {
        m_buffer[i].text.reserve(64);
        m_buffer[i].text = "";
        m_buffer[i].customAlpha = -1.0f; // Reset alpha

        m_buffer[i].visibleChars = 0;
        m_buffer[i].typeTimer = 0.0f;
        m_buffer[i].isDone = true;
    }

    m_writeIndex = 0;
    m_activeCount = 0;
}

void LogConsole::SetStyle(float scale, float lineSpacing, float r, float g, float b, float a)
{
    m_scale = scale;
    m_lineSpacing = lineSpacing;
    m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
}

void LogConsole::SetSpeed(float minDelay, float maxDelay)
{
    if (minDelay < 0.0f) minDelay = 0.0f;
    if (maxDelay < minDelay) maxDelay = minDelay;
    m_minDelay = minDelay;
    m_varDelay = maxDelay - minDelay;
}

void LogConsole::Update(float dt)
{
    // ==========================================
    // 1. Logic Spawn Log Baru (Timer)
    // ==========================================
    m_timer += dt;

    if (m_timer >= m_nextDelay)
    {
        m_timer = 0.0f;

        float randomVal = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        m_nextDelay = m_minDelay + (randomVal * m_varDelay);

        AddLog(GetRandomLog());
    }
    // <--- [PENTING] Tutup kurung timer di sini! 
    // Loop di bawah ini JANGAN dimasukkan ke dalam timer.

    // ==========================================
    // 2. Logic Typewriter (Jalan Tiap Frame)
    // ==========================================

    // Trik optimasi: Loop hanya pada log aktif
    int startIndex = (m_activeCount < m_maxLines) ? 0 : m_writeIndex;

    for (int i = 0; i < m_activeCount; ++i)
    {
        int idx = (startIndex + i) % m_maxLines;
        LogEntry& entry = m_buffer[idx];

        if (!entry.isDone)
        {
            entry.typeTimer += dt;

            // Waktunya nambah huruf?
            while (entry.typeTimer >= m_typeSpeed)
            {
                entry.typeTimer -= m_typeSpeed;
                entry.visibleChars++;

                // Cek apakah sudah selesai?
                if (entry.visibleChars >= entry.text.length())
                {
                    entry.visibleChars = (int)entry.text.length();
                    entry.isDone = true;
                    break; // Keluar dari while jika sudah selesai
                }
            }
        }
    }
}

// [OPTIMASI] Circular Buffer Logic
void LogConsole::AddLog(const std::string& message, float alphaOverride)
{
    // Simpan pesan DAN nilai alpha override ke struct
    m_buffer[m_writeIndex].text = message;
    m_buffer[m_writeIndex].customAlpha = alphaOverride;

    m_buffer[m_writeIndex].visibleChars = 0; // Belum ada huruf yang muncul
    m_buffer[m_writeIndex].typeTimer = 0.0f;
    m_buffer[m_writeIndex].isDone = false;   // Mulai ngetik!

    m_writeIndex = (m_writeIndex + 1) % m_maxLines;

    if (m_activeCount < m_maxLines) {
        m_activeCount++;
    }
}

std::string LogConsole::GetRandomLog()
{
    // [PENTING] Pakai 'const auto&' agar tidak meng-copy vector
    const auto& logs = TextDatabase::Instance().GetSystemLogs();
    if (logs.empty()) return "> System Check...";

    int idx = rand() % logs.size();

    // String concatenation sederhana
    return "> " + logs[idx];
}
void LogConsole::Render(BitmapFont* font)
{
    if (!font || m_activeCount == 0) return;

    // ... (Perhitungan startY sama seperti sebelumnya) ...
    float totalAreaHeight = m_maxLines * m_lineSpacing;
    float currentContentHeight = m_activeCount * m_lineSpacing;
    float startY = m_y + (totalAreaHeight - currentContentHeight);

    int startIndex = (m_activeCount < m_maxLines) ? 0 : m_writeIndex;
    float currentY = startY;

    // Buffer sementara untuk menggambar teks parsial
    // Static agar tidak alokasi memori ulang tiap frame
    static char drawBuffer[128];

    for (int i = 0; i < m_activeCount; ++i)
    {
        int actualIndex = (startIndex + i) % m_maxLines;
        const LogEntry& entry = m_buffer[actualIndex];

        float finalAlpha = m_color[3];
        if (entry.customAlpha >= 0.0f) finalAlpha = entry.customAlpha;

        // [BARU] Siapkan teks parsial
        if (entry.visibleChars > 0)
        {
            int len = entry.visibleChars;
            if (len >= 127) len = 127; // Safety cap

            // Copy karakter yang sudah "muncul" ke buffer
            // Kita pakai memcpy karena lebih cepat dari string copy
            memcpy(drawBuffer, entry.text.c_str(), len);

            // Tutup string dengan null terminator
            drawBuffer[len] = '\0';

            font->Draw(drawBuffer,
                m_x, currentY, m_scale,
                m_color[0], m_color[1], m_color[2], finalAlpha);
        }

        currentY += m_lineSpacing;
    }
}