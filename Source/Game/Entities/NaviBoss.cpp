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
    m_faceSprite = std::make_unique<Sprite>(device, "Data/Sprite/Boss/Sprite_Boss_Face_01.png");

    InitializeFaceGrid(device);
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

    // =========================================================
    // [FIX MUTLAK] CAMERA FILTERING WAJAH BOS
    // Pastikan tekstur ubin dan wajah UTAMA hanya digambar jika 
    // kamera yang sedang memproses saat ini adalah milik window "navi_head"!
    // =========================================================
    if (currentCamera == m_naviCamera.get())
    {
        // 1. RENDER MATRIKS 8x8 DULUAN (Sebagai Alas / Lapisan Dasar)
        RenderFaceGrid(context, currentCamera);

        // 2. RENDER UTAMA WAJAH (Mata, Mulut, Ekspresi) DI ATAS GRID
        if (m_faceSprite && m_naviWindow) {
            float unitW = m_windowSize.x / m_pixelToUnit;
            float unitH = m_windowSize.y / m_pixelToUnit;

            m_faceSprite->Render(
                context, currentCamera,
                m_position.x,
                m_position.y + 0.02f,
                m_position.z,
                unitW, unitH, DirectX::XMConvertToRadians(90.0f), 0.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            );
        }
    }

    // =========================================================
    // 3. OPER TUGAS MENGGAMBAR EFEK FASE (SAYAP, LASER, DLL)
    // Sengaja ditaruh di LUAR blok if kamera agar efek serangan/sayap 
    // tetap bisa dirender di window SFX layar penuh atau window portal lain!
    // =========================================================
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

    for (int r = 0; r < FACE_GRID_SIZE; ++r) {
        for (int c = 0; c < FACE_GRID_SIZE; ++c) {
            auto& tile = m_faceGrid[r][c];
            tile.timer += dt;

            // =========================================================
            // [NEW] EFEK FLICKER INSTAN (KEDIP CEPAT)
            // Mengecek peluang setiap frame. Jika tembus, ganti tekstur sedetik!
            // =========================================================
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

                // =========================================================
                // [NEW] EFEK GLITCH BRIGHTNESS & WARNA RGB
                // =========================================================
                if ((rand() % 100) < m_faceParams.colorGlitchChance) {
                    // Beri nilai acak antara 0.2 hingga 0.9 agar lebih redup (di bawah 1.0)
                    // Karena RGB-nya beda-beda, kadang jadi merah gelap, cyan kotor, dll!
                    tile.color.x = 0.2f + ((rand() % 70) / 100.0f); // Red
                    tile.color.y = 0.2f + ((rand() % 70) / 100.0f); // Green
                    tile.color.z = 0.2f + ((rand() % 70) / 100.0f); // Blue
                }
                else {
                    // Kembalikan ke warna normal (putih terang)
                    tile.color = { 1.0f, 1.0f, 1.0f };
                }
            }
        }
    }
}

void NaviBoss::RenderFaceGrid(ID3D11DeviceContext* context, Camera* currentCamera) {
    if (!currentCamera || m_faceTextures.empty()) return;

    DirectX::XMFLOAT3 bossPos = this->GetPosition();

    // =========================================================
        // [FIX MUTLAK] SINKRONISASI UKURAN 1:1 DENGAN WINDOW BOSS
        // Tidak perlu hitung ulang gelombang sin/cos!
        // Kita ambil langsung ukuran fisik yang digunakan oleh Window 
        // =========================================================

        // Ini akan mengambil ukuran OS Window yang sedang mengembang/mengempis (contoh: 400 / 40 = 10.0)
    float currentWindowUnitSize = m_windowSize.x / m_pixelToUnit;

    // Kita gunakan faceTotalSize (default 5.0) dari ImGui sebagai persen skala.
    // Jika slider di ImGui diset ke 5.0, ukurannya 100% pas menutupi window.
    // Jika slider di ImGui diturunkan, ubin akan sedikit lebih kecil dari window.
    float dynamicFaceSize = currentWindowUnitSize * (m_faceParams.faceTotalSize / 5.0f);

    float baseTileSize = dynamicFaceSize / FACE_GRID_SIZE;
    float startOffset = -dynamicFaceSize * 0.5f + baseTileSize * 0.5f;

    // =========================================================
    // MANAJEMEN TUMPANG TINDIH 2x2 (Tetap sama seperti kemarin)
    // =========================================================
    bool skipRender[FACE_GRID_SIZE][FACE_GRID_SIZE] = { false };

    for (int r = 0; r < FACE_GRID_SIZE; ++r) {
        for (int c = 0; c < FACE_GRID_SIZE; ++c) {
            if (skipRender[r][c]) continue;

            if (m_faceGrid[r][c].size == 2) {
                if (!skipRender[r + 1][c] && !skipRender[r][c + 1] && !skipRender[r + 1][c + 1]) {
                    skipRender[r + 1][c] = true;
                    skipRender[r][c + 1] = true;
                    skipRender[r + 1][c + 1] = true;
                }
                else {
                    m_faceGrid[r][c].size = 1;
                }
            }
        }
    }

    // =========================================================
    // PROSES PERULANGAN BATCH DATA GPU
    // =========================================================
    for (size_t texIdx = 0; texIdx < m_faceTextures.size(); ++texIdx) {
        std::vector<Sprite::Sprite3DBatchData> batchData;

        for (int r = 0; r < FACE_GRID_SIZE; ++r) {
            for (int c = 0; c < FACE_GRID_SIZE; ++c) {
                if (skipRender[r][c]) continue;

                auto& tile = m_faceGrid[r][c];

                if (tile.texIdx == static_cast<int>(texIdx)) {
                    float offsetX = startOffset + (c * baseTileSize);
                    float offsetZ = startOffset + (r * baseTileSize);

                    float w = baseTileSize;
                    float h = baseTileSize;

                    if (tile.size == 2) {
                        w = baseTileSize * 2.0f;
                        h = baseTileSize * 2.0f;
                        offsetX += baseTileSize * 0.5f;
                        offsetZ += baseTileSize * 0.5f;
                    }

                    batchData.push_back({
                        bossPos.x + offsetX,
                        bossPos.y + 0.01f,
                        bossPos.z + offsetZ,
                        w, h, 0.0f, 0.0f, 0.0f, 0.0f,
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