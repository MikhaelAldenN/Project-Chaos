#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"

using namespace DirectX;

NaviBoss::NaviBoss() {}
NaviBoss::~NaviBoss() {
    if (m_currentPhase) m_currentPhase->Exit(this);
}

void NaviBoss::Initialize(WindowTrackingSystem* windowSystem) {
    m_windowSystem = windowSystem;
    auto device = Graphics::Instance().GetDevice();

    // Setup Kepala Utama (Core) yang selalu ada
    TrackedWindowConfig headCfg = { "navi_head", "N.A.V.I - Core", (int)m_windowSize.x, (int)m_windowSize.y, 2 };
    headCfg.role = WindowRole::TRACKED_ENTITY;
    m_windowSystem->AddTrackedWindow(headCfg, [this]() { return m_position; }, [this]() { return m_windowSize; });

    auto* headWin = m_windowSystem->GetTrackedWindow("navi_head");
    if (headWin) {
        m_naviWindow = headWin->window;
        m_naviCamera = headWin->camera;
        m_naviWindow->SetDraggable(false);
    }
    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Placeholder/[PLACEHOLDER]Navi.png");

    // CATATAN: Kita tidak memanggil Initialize Phase di sini. 
    // SceneBoss yang akan menentukan fase mana yang mulai duluan via ChangePhase().
}

void NaviBoss::ChangePhase(std::unique_ptr<INaviPhase> newPhase) {
    if (m_currentPhase) {
        m_currentPhase->Exit(this);
    }

    m_currentPhase = std::move(newPhase);

    if (m_currentPhase) {
        m_currentPhase->Enter(this);
    }
}

void NaviBoss::Update(float dt) {
    m_glitchTimer += dt;

    if (m_windowSystem) {
        m_pixelToUnit = m_windowSystem->GetPixelToUnitRatio();
    }

    // Logika Breathing Kepala (Tetap di sini karena bersifat global)
    float breath = sinf(m_glitchTimer * m_breathSpeed) * m_breathIntensity;
    m_windowSize.x = m_baseWindowSize.x + breath;
    m_windowSize.y = m_baseWindowSize.y + breath;

    // Oper sisa logika ke fase aktif
    if (m_currentPhase) {
        m_currentPhase->Update(dt, this);
    }
}

void NaviBoss::Render(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!currentCamera) return;

    // Render Kepala (Selalu digambar)
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

    // Oper tugas menggambar efek fase ke kelas fase
    if (m_currentPhase) {
        m_currentPhase->Render(context, currentCamera, this);
    }
}