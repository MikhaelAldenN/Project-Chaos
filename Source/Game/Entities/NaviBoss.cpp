#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include <algorithm>
#include <random>

using namespace DirectX;

NaviBoss::NaviBoss() {}
NaviBoss::~NaviBoss() {}

void NaviBoss::Initialize(WindowTrackingSystem* windowSystem) {
    if (!windowSystem) return;

    auto device = Graphics::Instance().GetDevice();

    m_screenW = (float)GetSystemMetrics(SM_CXSCREEN);
    m_screenH = (float)GetSystemMetrics(SM_CYSCREEN);

    // 1. SETUP KEPALA NAVI
    TrackedWindowConfig headCfg = { "navi_head", "N.A.V.I - Core", (int)m_windowSize.x, (int)m_windowSize.y, 2 };
    headCfg.role = WindowRole::TRACKED_ENTITY;
    windowSystem->AddTrackedWindow(headCfg, [this]() { return m_position; }, [this]() { return m_windowSize; });

    auto* headWin = windowSystem->GetTrackedWindow("navi_head");
    if (headWin) {
        m_naviWindow = headWin->window;
        m_naviCamera = headWin->camera;
        m_naviWindow->SetDraggable(false);
    }
    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]Navi.png");

    // 2. SETUP GLOBAL FX WINDOW (FULLSCREEN & CLICKTHROUGH)
    TrackedWindowConfig fxCfg = { "navi_fx", "N.A.V.I - FX", (int)m_screenW, (int)m_screenH, 3 };
    fxCfg.role = WindowRole::SUB_VIEWPORT;
    fxCfg.isTransparent = true;

    windowSystem->AddTrackedWindow(fxCfg,
        []() { return DirectX::XMFLOAT3(0, 0, 0); },
        [this]() { return DirectX::XMFLOAT2(m_screenW, m_screenH); }
    );

    auto* fxWin = windowSystem->GetTrackedWindow("navi_fx");
    if (fxWin) {
        m_fxWindow = fxWin->window;
        m_fxCamera = fxWin->camera;
        m_fxWindow->SetBackgroundAlpha(0.0f);
        m_fxWindow->SetClickThrough(true);
        m_fxWindow->SetBorderVisible(false);
        m_fxWindow->SetDraggable(false);
    }

    m_wingSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]ErrorAtlas.png");
    GenerateButterflyWings();
}

void NaviBoss::ReplayAnimation() {
    m_wingState = WingState::Expanding;
    m_wingStateTimer = 0.0f;
    m_wingFlickerTimer = 0.0f;

    if (m_fxWindow) m_fxWindow->SetFPSLimit(0.0f); // Lepas limit saat mekar
    GenerateButterflyWings();
}

void NaviBoss::GenerateButterflyWings() {
    m_leftWingData.clear();
    m_rightWingData.clear();

    std::mt19937 gen(m_wingSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const int NODES_PER_WING = 120;

    for (int i = 0; i < NODES_PER_WING; ++i) {
        float linearT = (float)i / NODES_PER_WING;
        float t = pow(linearT, 2.0f);
        float angle = t * DirectX::XM_PI;

        float rEdge = (exp(cos(angle)) - 2 * cos(4 * angle) - pow(sin(angle / 12), 5)) * 3.0f;

        float randomVal = dist(gen);
        float randomFill = pow(randomVal, 0.3f);
        float r = (i % 5 == 0) ? rEdge : (rEdge * randomFill);

        float scatterX = (dist(gen) - 0.5f) * 1.5f;
        float scatterY = (dist(gen) - 0.5f) * 1.5f;

        WingNode node;
        node.localOffset = { 0.0f, 0.0f };
        node.targetOffset.y = (cos(angle) * r) + scatterY;
        node.flapOffset = t * 2.0f;

        float randomScale = 0.5f + dist(gen);
        float baseSize = 45.0f;
        node.size = { baseSize * randomScale, baseSize * randomScale };

        node.targetOffset.x = -abs(sin(angle) * r) + scatterX;
        m_leftWingData.push_back(node);

        node.targetOffset.x = abs(sin(angle) * r) - scatterX;
        m_rightWingData.push_back(node);
    }

    std::shuffle(m_leftWingData.begin(), m_leftWingData.end(), gen);
    std::shuffle(m_rightWingData.begin(), m_rightWingData.end(), gen);

    for (size_t i = 0; i < m_leftWingData.size(); ++i) {
        float linearDelay = ((float)i / m_leftWingData.size()) * m_spawnDuration;
        float randomOffset = (dist(gen) - 0.5f) * m_spawnChaos;
        m_leftWingData[i].spawnDelay = max(0.0f, linearDelay + randomOffset);
    }

    for (size_t i = 0; i < m_rightWingData.size(); ++i) {
        float linearDelay = ((float)i / m_rightWingData.size()) * m_spawnDuration;
        float randomOffset = (dist(gen) - 0.5f) * m_spawnChaos;
        m_rightWingData[i].spawnDelay = max(0.0f, linearDelay + randomOffset);
    }
}

void NaviBoss::Update(float dt) {
    m_glitchTimer += dt;

    if (m_wingState == WingState::Expanding) {
        m_wingStateTimer += dt;
        if (m_wingStateTimer >= WING_EXPAND_DURATION) {
            m_wingState = WingState::Idle;
            if (m_fxWindow) m_fxWindow->SetFPSLimit(0.0f);
        }
    }
    else {
        m_wingFlickerTimer += dt;
        if (m_wingFlickerTimer >= m_nextFlickerTarget) {
            m_wingFlickerTimer = 0.0f;
            float randomFactor = (rand() % 100) / 100.0f;
            m_nextFlickerTarget = 0.05f + (1.0f - min(1.0f, m_spawnChaos)) * 0.8f * randomFactor;
            int windowsToClose = 1 + (rand() % (int)(max(1.0f, m_spawnChaos * 5.0f)));

            for (int w = 0; w < windowsToClose; ++w) {
                std::vector<WingNode>& targetWing = (rand() % 2 == 0) ? m_leftWingData : m_rightWingData;
                if (targetWing.size() > 20) {
                    int randIdx = rand() % (targetWing.size() - 10);
                    targetWing[randIdx].isClosing = true;
                }
            }
        }
    }

    auto updateNodeAnimations = [&](std::vector<WingNode>& wingData) {
        int nodeToMoveToTop = -1;

        for (int i = 0; i < wingData.size(); ++i) {
            auto& node = wingData[i];

            if (m_wingState == WingState::Expanding) {
                if (m_wingStateTimer >= node.spawnDelay) {
                    node.animScale += dt / m_popDuration;
                    if (node.animScale > 1.0f) node.animScale = 1.0f;
                }
            }
            else {
                if (node.isClosing) {
                    node.animScale -= dt / m_popDuration;
                    if (node.animScale <= 0.0f) {
                        node.animScale = 0.0f;
                        node.isClosing = false;
                        nodeToMoveToTop = i;
                    }
                }
                else if (node.animScale < 1.0f) {
                    float individualSpeed = m_popDuration * (0.8f + ((rand() % 40) / 100.0f));
                    node.animScale += dt / individualSpeed;
                    if (node.animScale > 1.0f) node.animScale = 1.0f;
                }
            }
        }

        if (nodeToMoveToTop != -1) {
            WingNode temp = wingData[nodeToMoveToTop];
            wingData.erase(wingData.begin() + nodeToMoveToTop);
            wingData.push_back(temp);
        }
        };

    updateNodeAnimations(m_leftWingData);
    updateNodeAnimations(m_rightWingData);

    float breath = sinf(m_glitchTimer * m_breathSpeed) * m_breathIntensity;
    m_windowSize.x = m_baseWindowSize.x + breath;
    m_windowSize.y = m_baseWindowSize.y + breath;

    for (auto& node : m_leftWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;
        float flap = sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
        node.localOffset.x -= flap;
    }

    for (auto& node : m_rightWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;
        float flap = sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
        node.localOffset.x += flap;
    }
}

void NaviBoss::Render(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!currentCamera) return;

    bool isFXCam = (currentCamera == m_fxCamera.get());
    bool isMainCam = !isFXCam;

    if (m_faceSprite && m_naviWindow) {
        float unitW = m_windowSize.x / m_pixelToUnit;
        float unitH = m_windowSize.y / m_pixelToUnit;
        m_faceSprite->Render(
            context, currentCamera,
            m_position.x, m_position.y, m_position.z,
            unitW, unitH, DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        );
    }

    if (m_wingSprite && (isFXCam || isMainCam)) {
        std::vector<Sprite::Sprite3DBatchData> batchData;

        float leftWingX = m_position.x - m_wingXOffset;
        for (const auto& node : m_leftWingData) {
            if (node.animScale <= 0.0f) continue;
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;

            batchData.push_back({
                leftWingX + node.localOffset.x, m_position.y - 0.1f, m_position.z + m_wingZOffset + node.localOffset.y,
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }

        float rightWingX = m_position.x + m_wingXOffset;
        for (const auto& node : m_rightWingData) {
            if (node.animScale <= 0.0f) continue;
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;

            batchData.push_back({
                rightWingX + node.localOffset.x, m_position.y - 0.1f, m_position.z + m_wingZOffset + node.localOffset.y,
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }

        if (!batchData.empty()) {
            m_wingSprite->Render3DBatch(context, currentCamera, batchData);
        }
    }
}