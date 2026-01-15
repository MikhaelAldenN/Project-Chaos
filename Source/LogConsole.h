#pragma once
#include <vector>
#include <string>
#include "BitmapFont.h"

struct LogEntry {
    std::string text;       // Teks penuh
    float customAlpha = -1.0f;

    // [BARU] State Typewriter per baris
    int visibleChars = 0;   // Berapa huruf yang sudah muncul
    float typeTimer = 0.0f; // Timer internal untuk ngetik
    bool isDone = false;    // Apakah sudah selesai ngetik?
};

class LogConsole
{
public:
    LogConsole();
    ~LogConsole() = default;

    void Initialize(float x, float y, int maxLines);
    void SetStyle(float scale, float lineSpacing, float r, float g, float b, float a);
    void SetSpeed(float minDelay, float maxDelay);

    void Update(float dt);
    void Render(BitmapFont* font);
    void AddLog(const std::string& message, float alphaOverride = -1.0f);

private:
    // Layout
    float m_x = 0;
    float m_y = 0;
    float m_lineSpacing = 20.0f;
    float m_scale = 0.5f;
    float m_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

    // Buffer
    std::vector<LogEntry> m_buffer;
    int m_maxLines = 0;
    int m_writeIndex = 0;
    int m_activeCount = 0;

    // Logic Timer Spawn Log
    float m_timer = 0.0f;
    float m_nextDelay = 0.0f;
    float m_minDelay = 0.5f;
    float m_varDelay = 4.5f;

    // [BARU] Kecepatan Ngetik (Detik per huruf)
    // 0.01f = Cepat sekali
    // 0.05f = Normal
    float m_typeSpeed = 0.003f;

    std::string GetRandomLog();
};