#include "BossTerminal.h"
#include "BitmapFont.h"
#include "Camera.h"
#include <cmath>

using namespace DirectX;

// =========================================================
// LOCAL TEXT DATABASE (Encapsulated inside .cpp)
// =========================================================
namespace {
    namespace BossTextDB {
        const std::vector<std::string> SYSTEM_LOGS = {
            "SYS_CORE: Calculating trajectory...",
            "BUFFER_OVERFLOW: Memory leak detected at 0x4F",
            "TARGET_LOCK: Player entity found",
            "COMPILING: Shader_Destruction.hlsl",
            "WARNING: GPU Temperature Critical",
            "NETWORK: Connecting to External World...",
            "ANALYSIS: 4th Wall Integrity 45%",
            "USER_DATA: Fetching browser history...",
            "ERROR: Soul not found",
            "INIT: Boss_Pattern_Alpha.exe",
            "DEBUG: Infinite loop in ai_logic.cpp",
            "RENDERING: High Poly Mesh...",
            "> sudo rm -rf /player",
            "UPLOAD: Virus_Package_v2.zip",
            "SCANNING: Vulnerabilities..."
        };

        std::string GetRandomLog() {
            int idx = rand() % SYSTEM_LOGS.size();
            return "> " + SYSTEM_LOGS[idx];
        }
    }
}

// =========================================================
// IMPLEMENTATION
// =========================================================

void BossTerminal::Initialize(int maxLines)
{
    m_maxLines = maxLines;
    m_buffer.resize(maxLines);
    for (auto& entry : m_buffer) {
        entry.text.reserve(64);
        entry.isDone = true;
    }
}

void BossTerminal::AddLog(const std::string& message)
{
    BossLogEntry& entry = m_buffer[m_writeIndex];

    entry.text = message;
    entry.visibleChars = 0;
    entry.typeTimer = 0.0f;
    entry.isDone = false;

    m_writeIndex = (m_writeIndex + 1) % m_maxLines;
    if (m_activeCount < m_maxLines) m_activeCount++;
}

void BossTerminal::Update(float dt)
{
    // 1. Auto Spawn Logs
    m_spawnTimer += dt;
    if (m_spawnTimer >= m_nextSpawnDelay)
    {
        m_spawnTimer = 0.0f;
        m_nextSpawnDelay = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 1.5f;
        AddLog(BossTextDB::GetRandomLog());
    }

    // 2. Typewriter Effect
    int startIndex = (m_activeCount < m_maxLines) ? 0 : m_writeIndex;

    for (int i = 0; i < m_activeCount; ++i)
    {
        int idx = (startIndex + i) % m_maxLines;
        BossLogEntry& entry = m_buffer[idx];

        if (!entry.isDone)
        {
            entry.typeTimer += dt;
            while (entry.typeTimer >= m_typeSpeed)
            {
                entry.typeTimer -= m_typeSpeed;
                entry.visibleChars++;
                if (entry.visibleChars >= entry.text.length())
                {
                    entry.visibleChars = (int)entry.text.length();
                    entry.isDone = true;
                    break;
                }
            }
        }
    }
}

void BossTerminal::Render(BitmapFont* font, Camera* camera,
    const DirectX::XMFLOAT3& parentPos,
    const DirectX::XMFLOAT3& parentRot)
{
    if (!font || m_activeCount == 0) return;

    // 1. Setup Rotasi
    XMVECTOR vParentRot = XMLoadFloat3(&parentRot);

    XMVECTOR vOffsetRotDeg = XMLoadFloat3(&rotationOffset);
    XMVECTOR vOffsetRotRad = XMVectorScale(vOffsetRotDeg, XM_PI / 180.0f);

    XMVECTOR vFinalRotRad = XMVectorAdd(vParentRot, vOffsetRotRad);
    XMMATRIX matRot = XMMatrixRotationRollPitchYawFromVector(vFinalRotRad);

    // 2. Setup Posisi (ikut berputar mengikuti parent)
    XMVECTOR vOffsetPos = XMLoadFloat3(&offset);
    XMVECTOR vRotatedOffsetPos = XMVector3TransformCoord(vOffsetPos, matRot);
    XMVECTOR vFinalPos = XMVectorAdd(XMLoadFloat3(&parentPos), vRotatedOffsetPos);

    XMFLOAT3 startDrawPos;
    XMStoreFloat3(&startDrawPos, vFinalPos);

    XMFLOAT3 finalRotRadOfs;
    XMStoreFloat3(&finalRotRadOfs, vFinalRotRad);

    // 3. Render Loop
    static char drawBuf[128];
    int startIndex = (m_activeCount < m_maxLines) ? 0 : m_writeIndex;

    XMFLOAT3 downAxisLocal = { 0.0f, -1.0f, 0.0f };
    XMVECTOR vDownLocal = XMLoadFloat3(&downAxisLocal);
    XMVECTOR vDownWorld = XMVector3TransformNormal(vDownLocal, matRot);

    float actualSpacing = lineSpacing;

    for (int i = 0; i < m_activeCount; ++i)
    {
        int idx = (startIndex + i) % m_maxLines;
        const BossLogEntry& entry = m_buffer[idx];

        if (entry.visibleChars > 0)
        {
            int len = entry.visibleChars;
            if (len >= 127) len = 127;
            memcpy(drawBuf, entry.text.c_str(), len);
            drawBuf[len] = '\0';

            float dist = (float)i * actualSpacing;
            XMVECTOR vLineOffset = XMVectorScale(vDownWorld, dist);
            XMVECTOR vLinePos = XMVectorAdd(XMLoadFloat3(&startDrawPos), vLineOffset);

            XMFLOAT3 finalLinePos;
            XMStoreFloat3(&finalLinePos, vLinePos);

            font->Draw3D(drawBuf, camera, finalLinePos, scale, finalRotRadOfs, color);
        }
    }
}