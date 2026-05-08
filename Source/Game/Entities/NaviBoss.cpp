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

    // --- 1. Core Head Initialization ---
    TrackedWindowConfig headCfg = { "navi_head", "N.A.V.I - Core", (int)m_windowSize.x, (int)m_windowSize.y, 2 };
    headCfg.role = WindowRole::TRACKED_ENTITY;
    windowSystem->AddTrackedWindow(headCfg, [this]() { return m_position; }, [this]() { return m_windowSize; });

    if (auto* headWin = windowSystem->GetTrackedWindow("navi_head")) {
        m_naviWindow = headWin->window;
        m_naviCamera = headWin->camera;
        m_naviWindow->SetDraggable(false);
    }
    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]Navi.png");

    // --- 2. Left Wing Initialization ---
    TrackedWindowConfig lWingCfg = { "navi_wing_l", "Wing_L", (int)m_wingWindowSize.x, (int)m_wingWindowSize.y, 3 };
    lWingCfg.role = WindowRole::SUB_VIEWPORT;
    lWingCfg.isTransparent = true;
    windowSystem->AddTrackedWindow(lWingCfg,
        [this]() { return DirectX::XMFLOAT3(m_position.x - m_wingXOffset, m_position.y, m_position.z + m_wingZOffset); },
        [this]() { return m_wingWindowSize; }
    );

    if (auto* lWin = windowSystem->GetTrackedWindow("navi_wing_l")) {
        m_leftWingWindow = lWin->window;
        m_leftWingCamera = lWin->camera;
        m_leftWingWindow->SetBackgroundAlpha(0.0f);
        m_leftWingWindow->SetClickThrough(true);
        m_leftWingWindow->SetBorderVisible(false);
    }

    // --- 3. Right Wing Initialization ---
    TrackedWindowConfig rWingCfg = { "navi_wing_r", "Wing_R", (int)m_wingWindowSize.x, (int)m_wingWindowSize.y, 3 };
    rWingCfg.role = WindowRole::SUB_VIEWPORT;
    rWingCfg.isTransparent = true;
    windowSystem->AddTrackedWindow(rWingCfg,
        [this]() { return DirectX::XMFLOAT3(m_position.x + m_wingXOffset, m_position.y, m_position.z + m_wingZOffset); },
        [this]() { return m_wingWindowSize; }
    );

    if (auto* rWin = windowSystem->GetTrackedWindow("navi_wing_r")) {
        m_rightWingWindow = rWin->window;
        m_rightWingCamera = rWin->camera;
        m_rightWingWindow->SetBackgroundAlpha(0.0f);
        m_rightWingWindow->SetClickThrough(true);
        m_rightWingWindow->SetBorderVisible(false);
    }

    // --- 4. Asset Loading & Procedural Generation ---
    m_wingSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]ErrorAtlas.png");
    GenerateButterflyWings();
}

void NaviBoss::ReplayAnimation() {
    m_wingState = WingState::Expanding;
    m_wingStateTimer = 0.0f;
    m_wingFlickerTimer = 0.0f;

    // Release FPS lock during animation
    if (m_leftWingWindow) m_leftWingWindow->SetFPSLimit(0.0f);
    if (m_rightWingWindow) m_rightWingWindow->SetFPSLimit(0.0f);

    // Regenerate to apply any new spawn chaos/duration parameters
    GenerateButterflyWings();
}

void NaviBoss::GenerateButterflyWings() {
    m_leftWingData.clear();
    m_rightWingData.clear();

    std::mt19937 gen(m_wingSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const int NODES_PER_WING = 120;

    // --- 1. Geometric Distribution ---
    for (int i = 0; i < NODES_PER_WING; ++i) {
        // Biased angle distribution towards the upper section
        float linearT = (float)i / NODES_PER_WING;
        float t = pow(linearT, 2.0f);
        float angle = t * DirectX::XM_PI;

        // Butterfly curve perimeter radius
        float rEdge = (exp(cos(angle)) - 2 * cos(4 * angle) - pow(sin(angle / 12), 5)) * 3.0f;

        // Area fill distribution biased towards outer edges
        float randomFill = pow(dist(gen), 0.3f);
        float r = (i % 5 == 0) ? rEdge : (rEdge * randomFill);

        // Organic scatter noise
        float scatterX = (dist(gen) - 0.5f) * 1.5f;
        float scatterY = (dist(gen) - 0.5f) * 1.5f;

        WingNode node;
        node.localOffset = { 0.0f, 0.0f };
        node.targetOffset.y = (cos(angle) * r) + scatterY;
        node.flapOffset = t * 2.0f;

        float randomScale = 0.5f + dist(gen);
        float baseSize = 45.0f;
        node.size = { baseSize * randomScale, baseSize * randomScale };

        // Left Wing assignment
        node.targetOffset.x = -abs(sin(angle) * r) + scatterX;
        m_leftWingData.push_back(node);

        // Right Wing assignment
        node.targetOffset.x = abs(sin(angle) * r) - scatterX;
        m_rightWingData.push_back(node);
    }

    // --- 2. Z-Order Shuffle ---
    std::shuffle(m_leftWingData.begin(), m_leftWingData.end(), gen);
    std::shuffle(m_rightWingData.begin(), m_rightWingData.end(), gen);

    // --- 3. Staggered Delay Calculation ---
    auto applyDelays = [&](std::vector<WingNode>& wingData) {
        for (size_t i = 0; i < wingData.size(); ++i) {
            float linearDelay = ((float)i / wingData.size()) * m_spawnDuration;
            float randomOffset = (dist(gen) - 0.5f) * m_spawnChaos;
            wingData[i].spawnDelay = max(0.0f, linearDelay + randomOffset);
        }
        };

    applyDelays(m_leftWingData);
    applyDelays(m_rightWingData);
}

void NaviBoss::Update(float dt) {
    m_glitchTimer += dt;

    // --- 1. State Machine & Flicker Logic ---
    if (m_wingState == WingState::Expanding) {
        m_wingStateTimer += dt;

        // Finalize expansion phase
        if (m_wingStateTimer >= WING_EXPAND_DURATION) {
            m_wingState = WingState::Idle;
            // Throttle FPS to save CPU bandwidth when idle
            if (m_leftWingWindow) m_leftWingWindow->SetFPSLimit(20.0f);
            if (m_rightWingWindow) m_rightWingWindow->SetFPSLimit(20.0f);
        }
    }
    else {
        // Handle chaotic window closing/refreshing
        m_wingFlickerTimer += dt;

        if (m_wingFlickerTimer >= m_nextFlickerTarget) {
            m_wingFlickerTimer = 0.0f;

            // Determine next flicker interval based on chaos factor
            float randomFactor = (rand() % 100) / 100.0f;
            m_nextFlickerTarget = 0.05f + (1.0f - min(1.0f, m_spawnChaos)) * 0.8f * randomFactor;

            // Close multiple windows simultaneously based on chaos factor
            int windowsToClose = 1 + (rand() % (int)(max(1.0f, m_spawnChaos * 5.0f)));

            for (int w = 0; w < windowsToClose; ++w) {
                std::vector<WingNode>& targetWing = (rand() % 2 == 0) ? m_leftWingData : m_rightWingData;

                if (targetWing.size() > 20) {
                    // Target bottom/mid layers, avoiding top 10 elements
                    int randIdx = rand() % (targetWing.size() - 10);
                    targetWing[randIdx].isClosing = true;
                }
            }
        }
    }

    // --- 2. Node Animations ---
    auto updateNodeAnimations = [&](std::vector<WingNode>& wingData) {
        int nodeToMoveToTop = -1;

        for (int i = 0; i < wingData.size(); ++i) {
            auto& node = wingData[i];

            if (m_wingState == WingState::Expanding) {
                // Initial pop-in animation
                if (m_wingStateTimer >= node.spawnDelay) {
                    node.animScale += dt / m_popDuration;
                    if (node.animScale > 1.0f) node.animScale = 1.0f;
                }
            }
            else {
                if (node.isClosing) {
                    // Zoom-out animation
                    node.animScale -= dt / m_popDuration;
                    if (node.animScale <= 0.0f) {
                        node.animScale = 0.0f;
                        node.isClosing = false;
                        nodeToMoveToTop = i; // Queue for Z-order promotion
                    }
                }
                else if (node.animScale < 1.0f) {
                    // Zoom-in animation (refresh to top layer)
                    float individualSpeed = m_popDuration * (0.8f + ((rand() % 40) / 100.0f));
                    node.animScale += dt / individualSpeed;
                    if (node.animScale > 1.0f) node.animScale = 1.0f;
                }
            }
        }

        // Promote refreshed nodes to top rendering layer
        if (nodeToMoveToTop != -1) {
            WingNode temp = wingData[nodeToMoveToTop];
            wingData.erase(wingData.begin() + nodeToMoveToTop);
            wingData.push_back(temp);
        }
        };

    updateNodeAnimations(m_leftWingData);
    updateNodeAnimations(m_rightWingData);

    // --- 3. Kinematics Updates ---

    // Core Breathing
    float breath = sinf(m_glitchTimer * m_breathSpeed) * m_breathIntensity;
    m_windowSize.x = m_baseWindowSize.x + breath;
    m_windowSize.y = m_baseWindowSize.y + breath;

    // Wing Flapping & Interpolation
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

    // --- Camera Filtering ---
    bool isLeftWingCam = (currentCamera == m_leftWingCamera.get());
    bool isRightWingCam = (currentCamera == m_rightWingCamera.get());
    bool isMainCam = (!isLeftWingCam && !isRightWingCam);

    // --- Render Core (Visible to all valid cameras) ---
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

    // --- Render Left Wing (Batched) ---
    if (m_wingSprite && (isLeftWingCam || isMainCam)) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        float wingCenterX = m_position.x - m_wingXOffset;
        float wingCenterZ = m_position.z + m_wingZOffset;

        for (const auto& node : m_leftWingData) {
            if (node.animScale <= 0.0f) continue;

            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;

            batchData.push_back({
                wingCenterX + node.localOffset.x,
                m_position.y - 0.1f, // Apply Y-offset to prevent z-fighting
                wingCenterZ + node.localOffset.y,
                unitW, unitH,
                0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
                });
        }

        if (!batchData.empty()) {
            m_wingSprite->Render3DBatch(context, currentCamera, batchData);
        }
    }

    // --- Render Right Wing (Batched) ---
    if (m_wingSprite && (isRightWingCam || isMainCam)) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        float wingCenterX = m_position.x + m_wingXOffset;
        float wingCenterZ = m_position.z + m_wingZOffset;

        for (const auto& node : m_rightWingData) {
            if (node.animScale <= 0.0f) continue;

            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale * node.animScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale * node.animScale;

            batchData.push_back({
                wingCenterX + node.localOffset.x,
                m_position.y - 0.1f,
                wingCenterZ + node.localOffset.y,
                unitW, unitH,
                0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
                });
        }

        if (!batchData.empty()) {
            m_wingSprite->Render3DBatch(context, currentCamera, batchData);
        }
    }
}