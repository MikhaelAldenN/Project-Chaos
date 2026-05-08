#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"

using namespace DirectX;

NaviBoss::NaviBoss() {}
NaviBoss::~NaviBoss() {}

void NaviBoss::Initialize(WindowTrackingSystem* windowSystem) {
    if (!windowSystem) return;

    // 1. Konfigurasi Window Navi
    TrackedWindowConfig config;
    config.name = "navi_head";
    config.title = "N.A.V.I - System Monitor";
    config.width = static_cast<int>(m_windowSize.x);
    config.height = static_cast<int>(m_windowSize.y);
    config.priority = 2; // Di atas player, tapi di bawah GUI utama
    config.role = WindowRole::TRACKED_ENTITY;
    config.isTransparent = false; // Main window Navi biasanya punya border OS biar berasa "menginvasi"

    // 2. Registrasi ke Window System
    windowSystem->AddTrackedWindow(
        config,
        [this]() { return m_position; }, // Posisi tracking
        [this]() { return m_windowSize; } // Ukuran tracking
    );

    auto* tracked = windowSystem->GetTrackedWindow("navi_head");
    if (tracked) {
        m_naviWindow = tracked->window;
        m_naviCamera = tracked->camera;

        // Navi window biasanya tidak boleh di-drag manual oleh player
        m_naviWindow->SetDraggable(false);
    }

    // 3. Load Sprite (Gunakan placeholder jika file belum ada)
    auto device = Graphics::Instance().GetDevice();
    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]Navi.png");
}

void NaviBoss::Update(float dt) {
    m_glitchTimer += dt;

    // Sekarang menggunakan variabel dinamis dari header!
    float breath = sinf(m_glitchTimer * m_breathSpeed) * m_breathIntensity;

    m_windowSize.x = m_baseWindowSize.x + breath;
    m_windowSize.y = m_baseWindowSize.y + breath;
}

void NaviBoss::Render(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!m_faceSprite || !currentCamera || !m_naviWindow) return;

    // KEMBALIKAN KE m_windowSize (Ukuran Dinamis)
    // Sekarang sprite akan ikut membesar dan mengecil seiring tarikan napas window-nya
    float unitWidth = m_windowSize.x / 40.0f;
    float unitHeight = m_windowSize.y / 40.0f;

    m_faceSprite->Render(
        context,
        currentCamera,
        m_position.x, m_position.y, m_position.z,
        unitWidth, unitHeight,
        DirectX::XMConvertToRadians(90.0f),
        0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    );
}