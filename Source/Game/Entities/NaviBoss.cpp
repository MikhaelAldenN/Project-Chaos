#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"

using namespace DirectX;

NaviBoss::NaviBoss() {}
NaviBoss::~NaviBoss() {}

void NaviBoss::Initialize(WindowTrackingSystem* windowSystem) {
    if (!windowSystem) return;

    auto device = Graphics::Instance().GetDevice();

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

    // 2. SETUP SAYAP KIRI (Offset -10.0f X)
    TrackedWindowConfig lWingCfg = { "navi_wing_l", "Wing_L", (int)m_wingWindowSize.x, (int)m_wingWindowSize.y, 3 };
    lWingCfg.role = WindowRole::SUB_VIEWPORT;
    lWingCfg.isTransparent = true;
    windowSystem->AddTrackedWindow(lWingCfg,
        [this]() {
            // Jendela akan mengikuti posisi Navi + Offset yang kita atur
            return DirectX::XMFLOAT3(m_position.x - m_wingXOffset, m_position.y, m_position.z + m_wingZOffset);
        },
        [this]() { return m_wingWindowSize; }
    );
    auto* lWin = windowSystem->GetTrackedWindow("navi_wing_l");
    if (lWin) {
        m_leftWingWindow = lWin->window;
        m_leftWingCamera = lWin->camera;
        m_leftWingWindow->SetBackgroundAlpha(0.0f);
        m_leftWingWindow->SetClickThrough(true);
        m_leftWingWindow->SetBorderVisible(false);
    }

    // 3. SETUP SAYAP KANAN (Offset +10.0f X)
    TrackedWindowConfig rWingCfg = { "navi_wing_r", "Wing_R", (int)m_wingWindowSize.x, (int)m_wingWindowSize.y, 3 };
    rWingCfg.role = WindowRole::SUB_VIEWPORT;
    rWingCfg.isTransparent = true;
    windowSystem->AddTrackedWindow(rWingCfg,
        [this]() {
            return DirectX::XMFLOAT3(m_position.x + m_wingXOffset, m_position.y, m_position.z + m_wingZOffset);
        },
        [this]() { return m_wingWindowSize; }
    );
    auto* rWin = windowSystem->GetTrackedWindow("navi_wing_r");
    if (rWin) {
        m_rightWingWindow = rWin->window;
        m_rightWingCamera = rWin->camera;
        m_rightWingWindow->SetBackgroundAlpha(0.0f);
        m_rightWingWindow->SetClickThrough(true);
        m_rightWingWindow->SetBorderVisible(false);
    }

    // 4. LOAD GAMBAR SAYAP & GENERATE POLA
    m_wingSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]ErrorAtlas.png"); // Pakai gambar yang sama untuk tes
    GenerateButterflyWings();
}

void NaviBoss::GenerateButterflyWings() {
    m_leftWingData.clear();
    m_rightWingData.clear();
    const int NODES_PER_WING = 20;

    for (int i = 0; i < NODES_PER_WING; ++i) {
        float t = (float)i / NODES_PER_WING;
        float angle = t * DirectX::XM_PI;
        float r = (exp(cos(angle)) - 2 * cos(4 * angle) - pow(sin(angle / 12), 5)) * 3.0f;

        ErrorNode node;
        node.localOffset = { 0.0f, 0.0f };
        node.targetOffset.y = cos(angle) * r;
        node.flapOffset = t * 2.0f;

        // --- PERBAIKAN LOGIKA RANDOM SIZE ---
        // 1. Buat SATU pengali acak (misal antara 0.8x sampai 1.6x)
        float randomScale = 0.8f + ((rand() % 80) / 100.0f);

        // 2. Kalikan ukuran dasar dengan pengali yang SAMA untuk X dan Y
        node.size.x = m_wingNodeBaseSize.x * randomScale;
        node.size.y = m_wingNodeBaseSize.y * randomScale;

        // Kiri
        node.targetOffset.x = -abs(sin(angle) * r);
        m_leftWingData.push_back(node);

        // Kanan
        node.targetOffset.x = abs(sin(angle) * r);
        m_rightWingData.push_back(node);
    }
}

void NaviBoss::Update(float dt) {
    m_glitchTimer += dt;

    // --- DYNAMIC FPS THROTTLING LOGIC ---
    if (m_wingState == WingState::Expanding) {
        m_wingStateTimer += dt;

        // Setelah 2 detik sayap terbuka penuh di 60 FPS...
        if (m_wingStateTimer >= 2.0f) {
            m_wingState = WingState::Idle;

            // ...kunci FPS window sayap ke 20 untuk menyelamatkan CPU!
            if (m_leftWingWindow) m_leftWingWindow->SetFPSLimit(20.0f);
            if (m_rightWingWindow) m_rightWingWindow->SetFPSLimit(20.0f);
        }
    }

    // 1. Breathing Window Kepala (Tetap seperti sebelumnya)
    float breath = sinf(m_glitchTimer * m_breathSpeed) * m_breathIntensity;
    m_windowSize.x = m_baseWindowSize.x + breath;
    m_windowSize.y = m_baseWindowSize.y + breath;

    // 2. Animasikan Sayap Kiri
    for (auto& node : m_leftWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;

        // Gunakan parameter m_wingFlapSpeed dan m_wingFlapIntensity
        float flap = sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
        node.localOffset.x -= flap;
    }

    // 3. Animasikan Sayap Kanan
    for (auto& node : m_rightWingData) {
        node.localOffset.x += (node.targetOffset.x - node.localOffset.x) * dt * 2.0f;
        node.localOffset.y += (node.targetOffset.y - node.localOffset.y) * dt * 2.0f;

        // Gunakan parameter m_wingFlapSpeed dan m_wingFlapIntensity
        float flap = sinf(m_glitchTimer * m_wingFlapSpeed + node.flapOffset) * m_wingFlapIntensity;
        node.localOffset.x += flap;
    }
}

void NaviBoss::Render(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!currentCamera) return;

    // 1. Render Kepala (Muncul di semua window yang melihatnya)
    if (m_faceSprite && m_naviWindow) {
        float unitW = m_windowSize.x / m_pixelToUnit;
        float unitH = m_windowSize.y / m_pixelToUnit;
        m_faceSprite->Render(
            context, currentCamera,
            m_position.x, m_position.y, m_position.z,
            unitW, unitH, DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
        );
    }

    // --- LOGIKA FILTER KAMERA ---
    // Cek apakah kamera yang sedang merender adalah kamera sayap atau kamera utama (Player)
    bool isLeftWingCam = (currentCamera == m_leftWingCamera.get());
    bool isRightWingCam = (currentCamera == m_rightWingCamera.get());

    // Asumsikan kamera selain kamera sayap adalah kamera dunia/player
    bool isMainCam = (!isLeftWingCam && !isRightWingCam);

    // 2. Render Sayap Kiri
    if (m_wingSprite && (isLeftWingCam || isMainCam)) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        float wingCenterX = m_position.x - m_wingXOffset;
        float wingCenterZ = m_position.z + m_wingZOffset; // <--- Terapkan di sini

        for (const auto& node : m_leftWingData) {
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale;
            batchData.push_back({
                wingCenterX + node.localOffset.x, m_position.y - 0.1f, wingCenterZ + node.localOffset.y, // <--- Gunakan wingCenterZ
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }
        m_wingSprite->Render3DBatch(context, currentCamera, batchData);
    }

    // 3. Render Sayap Kanan
    if (m_wingSprite && (isRightWingCam || isMainCam)) {
        std::vector<Sprite::Sprite3DBatchData> batchData;
        float wingCenterX = m_position.x + m_wingXOffset;
        float wingCenterZ = m_position.z + m_wingZOffset; // <--- Terapkan di sini

        for (const auto& node : m_rightWingData) {
            float unitW = (node.size.x / m_pixelToUnit) * m_wingGlobalScale;
            float unitH = (node.size.y / m_pixelToUnit) * m_wingGlobalScale;
            batchData.push_back({
                wingCenterX + node.localOffset.x, m_position.y - 0.1f, wingCenterZ + node.localOffset.y, // <--- Gunakan wingCenterZ
                unitW, unitH, 0.0f, 0.0f, 0.0f, 0.0f,
                DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
                });
        }
        m_wingSprite->Render3DBatch(context, currentCamera, batchData);
    }
}