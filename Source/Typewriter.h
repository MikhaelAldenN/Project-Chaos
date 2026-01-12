#pragma once
#include <vector>
#include <string>
#include "BitmapFont.h"

struct TypewriterLine
{
    std::string text;
    float x = 0, y = 0;
    float fontSize = 1.0f;
    float color[4] = { 1, 1, 1, 1 };
    float postLineDelay = 0.2f;

    // Memory Test Specifics
    bool isMemoryTest = false;
    int memStart = 0;
    int memEnd = 0;
    int memCurrent = 0;
    std::string memSuffix;
    float memCurrentFloat = 0.0f;
    float memIncrementPerSecond = 0.0f;
};

class Typewriter
{
public:
    Typewriter() = default;
    ~Typewriter() = default;

    void AddLine(const std::string& text, float x, float y, float size, const float color[4], float delay = 0.2f);

    void AddMemoryTestLine(const std::string& prefix, int startVal, int endVal, float duration,
        const std::string& suffix, float x, float y, float size, const float color[4], float delay = 0.2f);

    bool Update(float dt);
    void Render(BitmapFont* font);
    void SkipCurrentLine();
    bool IsFinished() const;

private:
    TypewriterLine& CreateBaseLine(const std::string& text, float x, float y, float size, const float color[4], float delay);

    std::vector<TypewriterLine> lines;
    size_t currentLineIndex = 0;
    size_t currentCharIndex = 0;

    float charTimer = 0.0f;
    float lineDelayTimer = 0.0f;
    float typeSpeed = 0.001f;
};