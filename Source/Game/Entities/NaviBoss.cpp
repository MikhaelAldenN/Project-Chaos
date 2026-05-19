#include "NaviBoss.h"
#include "WindowTrackingSystem.h"
#include "System/Graphics.h"
#include "System/Sprite.h"
#include <SDL3/SDL.h> // Wajib ditambahkan di atas untuk manipulasi OS Window
#include <algorithm>

using namespace DirectX;

NaviBoss::NaviBoss() {}
NaviBoss::~NaviBoss() {
    if (m_currentPhase) m_currentPhase->Exit(this);
}

void NaviBoss::Initialize(WindowTrackingSystem* windowSystem) {
    m_windowSystem = windowSystem;
    auto device = Graphics::Instance().GetDevice();

    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Boss/Sprite_Boss_Face_01.png");
    InitializeFaceGrid(device);

    // [NEW] Daftarkan variasi nama material yang mudah ditambah/dikurangi di masa depan
    m_glitchTitles = {
        "mat_grass.png",
        "mat_stone.png",
        "mat_water.png",
        "mat_wood.png",
        "mat_metal.png",
        "mat_glass.png",
        "mat_magma.png",
        "mat_obsidian.png",
        "mat_error_null.png",
        "mat_fallback.png"
    };
}

void NaviBoss::SpawnHeadWindow() {
    if (!m_windowSystem) return;

    // Gunakan m_currentTitle (default: "mat_grass.png") sebagai nama awal window
    TrackedWindowConfig headCfg = { "navi_head", m_currentTitle, (int)m_windowSize.x, (int)m_windowSize.y, 2 };
    headCfg.role = WindowRole::TRACKED_ENTITY;
    m_windowSystem->AddTrackedWindow(headCfg, [this]() { return m_position; }, [this]() { return m_windowSize; });

    auto* headWin = m_windowSystem->GetTrackedWindow("navi_head");
    if (headWin) {
        m_naviWindow = headWin->window;
        m_naviCamera = headWin->camera;
        m_naviWindow->SetDraggable(false);
        m_naviWindow->SetClickThrough(true);
    }

    // Cari window berdasarkan title awal dan KUNCI handle-nya ke m_hHeadWindow
    m_hHeadWindow = FindWindowA(nullptr, m_currentTitle.c_str());
    if (m_hHeadWindow) {
        LONG style = GetWindowLong(m_hHeadWindow, GWL_STYLE);
        style &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        SetWindowLong(m_hHeadWindow, GWL_STYLE, style);

        HMENU hMenu = GetSystemMenu(m_hHeadWindow, FALSE);
        if (hMenu) {
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
        }

        LONG exStyle = GetWindowLong(m_hHeadWindow, GWL_EXSTYLE);
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
        SetWindowLong(m_hHeadWindow, GWL_EXSTYLE, exStyle);

        SetWindowPos(m_hHeadWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
    }
}

void NaviBoss::SetWindowTitle(const std::string& newTitle) {
    m_currentTitle = newTitle;
    if (m_hHeadWindow) {
        SetWindowTextA(m_hHeadWindow, m_currentTitle.c_str());
    }
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

    UpdateFaceGlitch(dt);

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

    if (currentCamera == m_naviCamera.get())
    {
        // Render fondasai ubin matriks glitch (Selama m_naviWindow valid / hidup)
        if (m_naviWindow) {
            RenderFaceGrid(context, currentCamera);
        }

        // Render tekstur wajah utama di layer atasnya hanya jika flag visibilitas diizinkan
        if (m_isFaceSpriteVisible && m_faceSprite && m_naviWindow) {
            float unitW = m_windowSize.x / m_pixelToUnit;
            float unitH = m_windowSize.y / m_pixelToUnit;

            m_faceSprite->Render(
                context, currentCamera,
                m_position.x, m_position.y + 0.02f, m_position.z,
                unitW, unitH, DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            );
        }
    }

    if (m_currentPhase) {
        m_currentPhase->Render(context, currentCamera, this);
    }
}

void NaviBoss::InitializeFaceGrid(ID3D11Device* device) {
    m_faceTextures.clear();

    // Sesuaikan jumlah total file Sprite_Boss_XX.png yang Anda miliki
    int totalSourceTextures = 14;

    for (int i = 1; i <= totalSourceTextures; ++i) {
        std::string numStr = (i < 10) ? "0" + std::to_string(i) : std::to_string(i);
        std::string path = "Data/Sprite/Boss/Sprite_Boss_" + numStr + ".png";
        m_faceTextures.push_back(std::make_unique<Sprite>(device, path.c_str()));
    }

    RandomizeFaceGrid();
}

void NaviBoss::RandomizeFaceGrid() {
    if (m_faceTextures.empty()) return;

    for (int r = 0; r < FACE_GRID_SIZE; ++r) {
        for (int c = 0; c < FACE_GRID_SIZE; ++c) {
            m_faceGrid[r][c].texIdx = rand() % m_faceTextures.size();

            // Hitung rentang fluktuasi acak berdasarkan parameter min-max baru
            float diff = m_faceParams.maxInterval - m_faceParams.minInterval;
            m_faceGrid[r][c].interval = m_faceParams.minInterval + ((rand() % 1000) / 1000.0f) * max(0.001f, diff);
            m_faceGrid[r][c].timer = ((rand() % 100) / 100.0f) * m_faceGrid[r][c].interval;
            m_faceGrid[r][c].size = 1;
        }
    }
}

void NaviBoss::UpdateFaceGlitch(float dt) {
    m_breathTimer += dt;
    if (m_faceTextures.empty() || !m_faceParams.enableGlitch) return;

    bool triggerTitleChange = false;

    for (int r = 0; r < FACE_GRID_SIZE; ++r) {
        for (int c = 0; c < FACE_GRID_SIZE; ++c) {
            auto& tile = m_faceGrid[r][c];
            tile.timer += dt;

            if (m_faceParams.flickerChance > 0.0f && ((rand() % 10000) / 100.0f) < m_faceParams.flickerChance) {
                tile.texIdx = rand() % m_faceTextures.size();
            }

            if (tile.timer >= tile.interval) {
                tile.timer = 0.0f;
                tile.texIdx = rand() % m_faceTextures.size();

                float diff = m_faceParams.maxInterval - m_faceParams.minInterval;
                tile.interval = m_faceParams.minInterval + ((rand() % 1000) / 1000.0f) * max(0.001f, diff);

                if (r < FACE_GRID_SIZE - 1 && c < FACE_GRID_SIZE - 1 && (rand() % 100) < m_faceParams.chance2x2) {
                    tile.size = 2;
                }
                else {
                    tile.size = 1;
                }

                if ((rand() % 100) < m_faceParams.colorGlitchChance) {
                    tile.color.x = 0.2f + ((rand() % 70) / 100.0f);
                    tile.color.y = 0.2f + ((rand() % 70) / 100.0f);
                    tile.color.z = 0.2f + ((rand() % 70) / 100.0f);
                }
                else {
                    tile.color = { 1.0f, 1.0f, 1.0f };
                }

                // Jika ada ubin yang berganti siklus intervalnya, tandai bahwa ritme glitch sedang berjalan
                triggerTitleChange = true;
            }
        }
    }

    // [NEW] Sinkronisasi Judul: Jika ritme ubin bergerak, berikan peluang acak untuk mengacak judul material
    if (triggerTitleChange && !m_glitchTitles.empty()) {
        // Peluang 8% agar pergantian judul bar tidak terlalu merusak performa OS akibat penggantian teks yang terlalu rapat
        if ((rand() % 100) < 8) {
            int randIdx = rand() % m_glitchTitles.size();
            SetWindowTitle(m_glitchTitles[randIdx]);
        }
    }
}

void NaviBoss::RenderFaceGrid(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!currentCamera || m_faceTextures.empty()) return;

    DirectX::XMFLOAT3 bossPos = this->GetPosition();
    float currentWindowUnitSize = m_windowSize.x / m_pixelToUnit;
    float dynamicFaceSize = currentWindowUnitSize * (m_faceParams.faceTotalSize / 5.0f);

    // [DYNAMIC DENSITY HACK] Tentukan berapa baris/kolom pecahan yang aktif saat ini (1 s/d 8)
    int currentLimit = std::clamp((int)m_currentGridLimit, 1, FACE_GRID_SIZE);

    // Ukuran ubin membesar secara matematis jika pembaginya (currentLimit) masih kecil
    float baseTileSize = dynamicFaceSize / currentLimit;
    float startOffset = -dynamicFaceSize * 0.5f + baseTileSize * 0.5f;

    for (size_t texIdx = 0; texIdx < m_faceTextures.size(); ++texIdx) {
        std::vector<Sprite::Sprite3DBatchData> batchData;

        for (int r = 0; r < currentLimit; ++r) {
            for (int c = 0; c < currentLimit; ++c) {

                // Petakan koordinat looping dinamis secara proporsional ke indeks data grid 8x8 asli
                int srcR = (r * FACE_GRID_SIZE) / currentLimit;
                int srcC = (c * FACE_GRID_SIZE) / currentLimit;
                auto& tile = m_faceGrid[srcR][srcC];

                if (tile.texIdx == static_cast<int>(texIdx)) {
                    float offsetX = startOffset + (c * baseTileSize);
                    float offsetZ = startOffset + (r * baseTileSize);

                    batchData.push_back({
                        bossPos.x + offsetX,
                        bossPos.y + 0.01f,
                        bossPos.z + offsetZ,
                        baseTileSize, baseTileSize, 0.0f, 0.0f, 0.0f, 0.0f,
                        DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
                        tile.color.x, tile.color.y, tile.color.z, 1.0f
                        });
                }
            }
        }

        if (!batchData.empty()) {
            m_faceTextures[texIdx]->Render3DBatch(context, currentCamera, batchData);
        }
    }
}