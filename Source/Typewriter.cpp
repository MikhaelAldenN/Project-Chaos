#include "Typewriter.h"

TypewriterLine& Typewriter::CreateBaseLine(const std::string& text, float x, float y, float size, const float color[4], float delay)
{
    TypewriterLine line;
    line.text = text;
    line.x = x;
    line.y = y;
    line.fontSize = size;
    line.postLineDelay = delay;
    for (int i = 0; i < 4; i++) line.color[i] = color[i];

    lines.push_back(line);
    return lines.back();
}

void Typewriter::AddLine(const std::string& text, float x, float y, float size, const float color[4], float delay)
{
    CreateBaseLine(text, x, y, size, color, delay);
}

void Typewriter::AddMemoryTestLine(const std::string& prefix, int startVal, int endVal, float duration,
    const std::string& suffix, float x, float y, float size, const float color[4], float delay)
{
    TypewriterLine& line = CreateBaseLine(prefix, x, y, size, color, delay);

    line.isMemoryTest = true;
    line.memStart = startVal;
    line.memEnd = endVal;
    line.memCurrent = startVal;
    line.memCurrentFloat = static_cast<float>(startVal);
    line.memSuffix = suffix;

    // Prevent division by zero
    if (duration > 0.0f)
        line.memIncrementPerSecond = (float)(endVal - startVal) / duration;
    else
        line.memIncrementPerSecond = (float)(endVal - startVal); // Instant
}

bool Typewriter::Update(float dt)
{
    if (IsFinished()) return false;

    TypewriterLine& currentLine = lines[currentLineIndex];
    bool isTyping = false;

    // Type Main Text
    if (currentCharIndex < currentLine.text.size())
    {
        charTimer += dt;
        while (charTimer >= typeSpeed)
        {
            charTimer -= typeSpeed;
            currentCharIndex++;
            isTyping = true;
            if (currentCharIndex >= currentLine.text.size()) {
                charTimer = 0.0f;
                break;
            }
        }
    }

    // Memory Test Counter
    else if (currentLine.isMemoryTest && currentLine.memCurrent < currentLine.memEnd)
    {
        currentLine.memCurrentFloat += currentLine.memIncrementPerSecond * dt;
        currentLine.memCurrent = static_cast<int>(currentLine.memCurrentFloat);

        if (currentLine.memCurrent >= currentLine.memEnd)
            currentLine.memCurrent = currentLine.memEnd;

        isTyping = true;
    }

	// Delay Before Next Line
    else
    {
        lineDelayTimer += dt;
        if (lineDelayTimer >= currentLine.postLineDelay)
        {
            lineDelayTimer = 0.0f;
            currentLineIndex++;
            currentCharIndex = 0;
        }
    }

    return isTyping;
}

void Typewriter::Render(BitmapFont* font)
{
    if (!font) return;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i > currentLineIndex) break;

        const TypewriterLine& line = lines[i];

        std::string textToDraw;
        if (i < currentLineIndex)
        {
            textToDraw = line.text;
        }
        else
        {
            size_t len = (currentCharIndex < line.text.size()) ? currentCharIndex : line.text.size();
            textToDraw = line.text.substr(0, len);
        }

        bool prefixDone = (i < currentLineIndex) || (currentCharIndex >= line.text.size());
        if (line.isMemoryTest && prefixDone)
        {
            textToDraw += std::to_string(line.memCurrent) + line.memSuffix;
        }

        font->Draw(textToDraw.c_str(), line.x, line.y, line.fontSize,
            line.color[0], line.color[1], line.color[2], line.color[3]);
    }
}

void Typewriter::SkipCurrentLine()
{
    if (IsFinished()) return;

    TypewriterLine& line = lines[currentLineIndex];
    currentCharIndex = line.text.size();

    if (line.isMemoryTest)
        line.memCurrent = line.memEnd;
}

bool Typewriter::IsFinished() const
{
    return currentLineIndex >= lines.size();
}