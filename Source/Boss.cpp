#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Player.h"

using namespace DirectX;

// =========================================================
// BOSS PART IMPLEMENTATION
// =========================================================

void BossPart::Update(float dt)
{
    if (!model) return;

    // Floating Animation
    float offX = 0, offY = 0, offZ = 0;
    if (useFloating)
    {
        timer += dt;
        float wave = sinf(timer * floatSpeed) * floatIntensity;
        offX = wave * floatAxis.x;
        offY = wave * floatAxis.y;
        offZ = wave * floatAxis.z;
    }
    visualPosition = { position.x + offX, position.y + offY, position.z + offZ };

    // Build Matrix
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation.x), XMConvertToRadians(rotation.y), XMConvertToRadians(rotation.z));
    XMMATRIX T = XMMatrixTranslation(visualPosition.x, visualPosition.y, visualPosition.z);

    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, S * R * T);
    model->UpdateTransform(world);
}

void BossPart::Render(ModelRenderer* renderer)
{
    if (model) renderer->Draw(ShaderId::Lambert, model, { 1.0f, 1.0f, 1.0f, 1.0f });
}

// =========================================================
// BOSS MAIN CLASS
// =========================================================

Boss::Boss()
{
    InitializeParts();

    auto device = Graphics::Instance().GetDevice();

    // Init Terminals
    m_terminal.Initialize(device, 512, 512);
    m_terminal.AddLog("SYSTEM BOOT...");
    m_terminal1.Initialize(device, 512, 512);

    // Init Visuals
    m_bgChainSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Beyond/Sprite_BackChain.png");

    // Init Models (Safe Load)
    auto LoadModel = [&](const char* path) -> std::shared_ptr<Model> {
        try { return std::make_shared<Model>(device, path); }
        catch (...) { OutputDebugStringA(("Failed to load: " + std::string(path) + "\n").c_str()); return nullptr; }
        };

    m_screenQuad = LoadModel("Data/Model/Primitive/Plane.glb");
    m_screenQuad1 = LoadModel("Data/Model/Primitive/Plane.glb");
    m_fileModel = LoadModel("Data/Model/Character/Test_mdl_File.glb");
    m_wireModel1 = LoadModel("Data/Model/Character/TEST_mdl_ElectricWire1.glb");
    m_wireModel2 = LoadModel("Data/Model/Character/TEST_mdl_ElectricWire2.glb");
    m_wires.resize(MAX_WIRES);
    for (int i = 0; i < MAX_WIRES; ++i) {
        m_wires[i].id = i;
        m_wires[i].state = WireState::INACTIVE;
    }

    m_fileProjectiles.reserve(100);
    m_stateMachine.Initialize(new BossIntroState(), this);
}

Boss::~Boss() {}

void Boss::InitializeParts()
{
    std::vector<BossPartConfig> configs = {
        { "monitor1", "Data/Model/Character/TEST_mdl_CRTMonitor.glb", { 0.f, 0.6f, 6.5f }, { 65.f, 0.f, 0.f }, { 10.f, 10.f, 10.f }, true, 2.0f, 0.2f, { 0.f, 1.f, 0.f } },
        { "cpu",      "Data/Model/Character/TEST_mdl_CPU.glb",        { 2.f, -1.6f, 1.9f }, { -86.f, 0.f, 176.f }, { 150.f, 150.f, 150.f }, true, 1.5f, 0.2f, { 1.f, 1.f, 0.f }, 4.5f, {0.f, 10.f, 0.f} },
        { "monitor2", "Data/Model/Character/Test_mdl_CRTMonitor2.glb", { 5.5f, 0.f, 4.2f }, { -44.f, 147.f, 31.f }, { 100.f, 100.f, 100.f }, true, 1.0f, 0.05f, { 1.f, 0.f, -1.f } },
        { "monitor3", "Data/Model/Character/TEST_mdl_CRTMonitor3.glb", { 5.f, 2.3f, 7.5f }, { 110.f, 123.f, 128.f }, { 100.f, 100.f, 100.f }, true, 1.0f, 0.25f, { 0.5f, 0.5f, 0.5f } },
        { "cable1",   "Data/Model/Character/TEST_mdl_Cable1.glb",      { -8.6f, 0.f, 13.9f }, { 90.f, 0.f, 0.f }, { 10.f, 10.f, 10.f }, true, 1.0f, 1.0f, { 1.f, -1.f, 0.f } },
        { "cable2",   "Data/Model/Character/TEST_mdl_Cable1.glb",      { 6.3f, 5.5f, 12.9f }, { 90.f, 0.f, 0.f }, { 8.f, 8.f, 8.f }, true, 0.5f, 0.5f, { -1.f, 1.f, 0.f } },
        { "cable3",   "Data/Model/Character/TEST_mdl_Cable3.glb",      { 2.f, 0.7f, 3.0f }, { 156.f, -16.f, -133.f }, { 0.03f, 0.03f, 0.03f }, true, 2.0f, 0.2f, { 0.f, 1.f, 0.f } },
        { "platform1","Data/Model/Character/TEST_mdl_Platform.glb",    { -9.6f, 11.2f, 8.7f }, { -62.f, 138.f, 48.f }, { 2.f, 2.f, 2.f }, true, 1.0f, 0.2f, { 0.f, 1.f, 0.f } },
        { "platform2","Data/Model/Character/TEST_mdl_Platform.glb",    { 8.3f, 11.2f, 9.2f }, { 43.f, 60.f, 59.f }, { 2.f, 2.f, 2.f }, true, 1.0f, 0.2f, { 0.f, 1.f, 0.f } },
        { "antenna",  "Data/Model/Character/TEST_mdl_Antenna.glb",     m_antennaConfig.hiddenPos, m_antennaConfig.rotation, m_antennaConfig.scale, false }
    };

    auto device = Graphics::Instance().GetDevice();
    for (const auto& cfg : configs)
    {
        auto part = std::make_unique<BossPart>();
        part->name = cfg.name;

        try { part->model = std::make_shared<Model>(device, cfg.modelPath.c_str()); }
        catch (...) { continue; }

        part->position = cfg.position;
        part->rotation = cfg.rotation;
        part->scale = cfg.scale;
        part->useFloating = cfg.useFloating;
        part->floatSpeed = cfg.floatSpeed;
        part->floatIntensity = cfg.floatIntensity;
        part->floatAxis = cfg.floatAxis;
        part->cullRadius = cfg.cullRadius;
        part->cullOffset = cfg.cullOffset;

        // =========================================================
        // [FIX] MATERIAL UNLINKING (SOLUSI BUG GAMBAR TERTUKAR)
        // =========================================================
        // Kita buat fungsi lambda kecil untuk menduplikasi material
        auto MakeMaterialUnique = [&](std::shared_ptr<Model> model)
            {
                if (!model) return;

                // 1. Buka gembok 'const' agar vektor mesh bisa diedit
                auto& mutableMeshes = const_cast<std::vector<Model::Mesh>&>(model->GetMeshes());

                for (auto& mesh : mutableMeshes)
                {
                    if (mesh.material)
                    {
                        // 2. Buat Copy Material Baru
                        auto uniqueMat = std::make_shared<Model::Material>(*mesh.material);

                        // 3. SIMPAN di Cache Boss agar memori tidak hilang (PENTING!)
                        m_materialCache.push_back(uniqueMat);

                        // 4. Pasang ke mesh menggunakan Raw Pointer (.get())
                        mesh.material = uniqueMat.get();
                    }
                }
            };

        // Terapkan ke kedua layar
        MakeMaterialUnique(m_screenQuad);
        MakeMaterialUnique(m_screenQuad1);
        // =========================================================

        m_partMap[cfg.name] = part.get();
        m_parts.push_back(std::move(part));
    }
}

// =========================================================
// UPDATE LOGIC
// =========================================================

void Boss::Update(float dt)
{
    float safeDt = min(dt, 0.05f);

    for (auto& part : m_parts) part->Update(safeDt);

    m_terminal.Update(dt);
    m_terminal1.Update(dt);
    m_stateMachine.Update(this, safeDt);

    UpdateBackgroundAnim(dt);
    UpdateWires(safeDt);
}

void Boss::UpdateBackgroundAnim(float dt)
{
    bool visible = m_terminal1.IsSystemLocked() || m_debugForceBG;

    if (visible && (m_bgState == BgAnimState::HIDDEN || m_bgState == BgAnimState::EXITING))
    {
        m_bgState = BgAnimState::ENTERING;
        m_bgTimer = 0.0f;
    }
    else if (!visible && (m_bgState == BgAnimState::ENTERING || m_bgState == BgAnimState::ACTIVE))
    {
        m_bgState = BgAnimState::EXITING;
    }

    switch (m_bgState)
    {
    case BgAnimState::HIDDEN: m_bgAlpha = 0.0f; break;
    case BgAnimState::ENTERING:
        m_bgTimer += dt;
        if (m_bgTimer <= 0.1f) m_bgAlpha = m_bgTimer / 0.1f;
        else if (m_bgTimer <= 0.4f) m_bgAlpha = 1.0f + (0.5f - 1.0f) * ((m_bgTimer - 0.1f) / 0.3f);
        else { m_bgAlpha = 0.5f; m_bgState = BgAnimState::ACTIVE; }
        break;
    case BgAnimState::ACTIVE: m_bgAlpha = 0.5f; break;
    case BgAnimState::EXITING:
        m_bgAlpha -= 2.0f * dt;
        if (m_bgAlpha <= 0.0f) { m_bgAlpha = 0.0f; m_bgState = BgAnimState::HIDDEN; }
        break;
    }
}

// =========================================================
// RENDER LOGIC
// =========================================================

void Boss::Render(ModelRenderer* renderer, Camera* camera)
{
    auto ctx = Graphics::Instance().GetDeviceContext();
    auto font = ResourceManager::Instance().GetFont("VGA_FONT");

    // 1. Terminals
    if (font) {
        m_terminal.RenderToTexture(ctx, font);
        m_terminal1.RenderToTexture(ctx, font);
    }

    // 2. Background Chain
    if (m_bgAlpha > 0.0f && m_bgChainSprite && camera)
    {
        auto rs = Graphics::Instance().GetRenderState();
        ctx->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        float blend[4] = { 0,0,0,0 };
        ctx->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blend, 0xFFFFFFFF);

        m_bgChainSprite->Render(ctx, camera, 0, 0, 0, 48, 26, XMConvertToRadians(90), 0, 0, 1, 1, 1, m_bgAlpha);

        ctx->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
        ctx->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), blend, 0xFFFFFFFF);
    }

    // 3. Body Parts (With Culling)
    for (auto& part : m_parts)
    {
        if (camera) {
            auto pos = part->visualPosition;
            if (!camera->CheckSphere(pos.x + part->cullOffset.x, pos.y + part->cullOffset.y, pos.z + part->cullOffset.z, part->cullRadius)) continue;
        }
        part->Render(renderer);
    }

    RenderScreens(renderer);
    RenderProjectiles(renderer, camera);
    RenderWires(renderer);
}

void Boss::RenderScreens(ModelRenderer* renderer)
{
    // Ubah parameter ke-2 jadi std::shared_ptr<Model>
    auto RenderScreen = [&](BossPart* monitor, std::shared_ptr<Model> screen, const ScreenTransform& cfg, ID3D11ShaderResourceView* tex)
        {
            XMMATRIX matMon = XMMatrixScaling(monitor->scale.x, monitor->scale.y, monitor->scale.z) *
                XMMatrixRotationRollPitchYaw(XMConvertToRadians(monitor->rotation.x), XMConvertToRadians(monitor->rotation.y), XMConvertToRadians(monitor->rotation.z)) *
                XMMatrixTranslation(monitor->visualPosition.x, monitor->visualPosition.y, monitor->visualPosition.z);

            XMMATRIX matScr = XMMatrixScaling(cfg.scale.x, cfg.scale.y, cfg.scale.z) *
                XMMatrixRotationRollPitchYaw(XMConvertToRadians(cfg.rotation.x), XMConvertToRadians(cfg.rotation.y), XMConvertToRadians(cfg.rotation.z)) *
                XMMatrixTranslation(cfg.offset.x, cfg.offset.y, cfg.offset.z);

            XMFLOAT4X4 world;
            XMStoreFloat4x4(&world, matScr * matMon);

            screen->UpdateTransform(world);

            if (tex)
            {
                for (const auto& mesh : screen->GetMeshes())
                    if (mesh.material) mesh.material->baseMap = tex;
            }

            // [FIX] Gunakan 'screen' (variabel parameter), BUKAN m_fileModel
            renderer->Draw(ShaderId::Basic, screen, DirectX::XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), world);
        };


    // Panggil dengan passing m_screenQuad (Shared Ptr) langsung
    if (m_screenQuad && HasPart("monitor2"))
        RenderScreen(GetPart("monitor2"), m_screenQuad, m_screen2Config, m_terminal.GetTexture());

    if (m_screenQuad1 && HasPart("monitor1"))
        RenderScreen(GetPart("monitor1"), m_screenQuad1, m_screen1Config, m_terminal1.GetTexture());
}

void Boss::RenderProjectiles(ModelRenderer* renderer, Camera* camera)
{
    if (!m_fileModel) return;

    for (const auto& p : m_fileProjectiles)
    {
        if (!p.active) continue;
        if (camera && !camera->CheckSphere(p.position.x, p.position.y, p.position.z, 1.5f)) continue;

        XMMATRIX S = XMMatrixScaling(150.f, 150.f, 150.f);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(p.rotation.x), XMConvertToRadians(p.rotation.y), XMConvertToRadians(p.rotation.z));
        XMMATRIX T = XMMatrixTranslation(p.position.x, p.position.y, p.position.z);

        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, S * R * T);
        renderer->Draw(ShaderId::Lambert, m_fileModel, { 0,1,1,1 }, world);
    }
}

// =========================================================
// SYSTEM API
// =========================================================

BossPart* Boss::GetPart(const std::string& name)
{
    auto it = m_partMap.find(name);
    return (it != m_partMap.end()) ? it->second : nullptr;
}

bool Boss::HasPart(const std::string& name) const
{
    return m_partMap.find(name) != m_partMap.end();
}

DirectX::XMFLOAT3 Boss::GetPartVisualPos(const std::string& name) const
{
    auto* p = const_cast<Boss*>(this)->GetPart(name);
    return p ? p->visualPosition : DirectX::XMFLOAT3{ 0,0,0 };
}

void Boss::ChangeState(BossState* newState) { m_stateMachine.ChangeState(this, newState); }
void Boss::TriggerIdle() { ChangeState(new BossIdleState()); }
void Boss::TriggerSpawnEnemy() { ChangeState(new BossCommandState(new BossSpawnEnemyState(), "DEPLOY MINIONS")); }
void Boss::TriggerLockPlayer() { ChangeState(new BossCommandState(new BossLockPlayerState(), "INITIATE LOCK")); }
void Boss::TriggerSpawnPentagon() { ChangeState(new BossCommandState(new BossSpawnPentagonState(), "DEPLOYING HEAVY UNIT")); }
void Boss::AddTerminalLog(const std::string& msg) { m_terminal.AddLog(msg); }

void Boss::SpawnFileProjectile(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& target)
{
    // Try reuse existing
    for (auto& p : m_fileProjectiles) {
        if (!p.active) {
            p = FileProjectile{ p.id, true, start, {float(rand() % 360), float(rand() % 360), float(rand() % 360)}, target, 25.0f };
            return;
        }
    }
    // Create new
    if (m_fileProjectiles.size() < 100) {
        m_fileProjectiles.push_back({ m_projectileIdCounter++, true, start, {float(rand() % 360), float(rand() % 360), float(rand() % 360)}, target, 25.0f });
    }
}

void Boss::ClearProjectiles() { m_fileProjectiles.clear(); }

bool Boss::GetProjectileData(int id, DirectX::XMFLOAT3& outPos) const
{
    for (const auto& p : m_fileProjectiles) if (p.active && p.id == id) { outPos = p.position; return true; }
    return false;
}

// =========================================================
// DEBUG GUI
// =========================================================

void Boss::DrawDebugGUI()
{
    if (!ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (ImGui::BeginTabBar("BossManagerTabs"))
    {
        // TAB 1: GENERAL
        if (ImGui::BeginTabItem("General"))
        {
            ImGui::Separator();

            // Status Indicators
            ImGui::Text("Terminal 1: %s", (m_screenQuad1 && HasPart("monitor1")) ? "OK" : "ERR");
            ImGui::Text("Terminal 2: %s", (m_screenQuad && HasPart("monitor2")) ? "OK" : "ERR");

            ImGui::Separator();
            m_terminal.DrawGUI();
            m_terminal1.DrawGUI();

            ImGui::Separator();
            if (ImGui::Button("IDLE")) TriggerIdle(); ImGui::SameLine();
            if (ImGui::Button("SPAWN ENEMY")) TriggerSpawnEnemy(); ImGui::SameLine();
            if (ImGui::Button("LOCK PLAYER")) TriggerLockPlayer(); ImGui::SameLine();
            if (ImGui::Button("DOWNLOAD")) ChangeState(new BossCommandState(new BossDownloadAttackState(), "DOWNLOADING..."));

            ImGui::Separator();
            if (ImGui::Button("SPAWN PENTAGON")) TriggerSpawnPentagon(); ImGui::SameLine();
            if (ImGui::Button("WIRE ATTACK"))
            {
                // 1. Buat State Serangan (Wire Attack)
                BossState* attackState = new BossWireAttackState();

                // 2. Bungkus dengan CommandState (Untuk efek ngetik "INITIATING SURGE...")
                // BossCommandState akan otomatis pindah ke attackState setelah ngetik selesai.
                ChangeState(new BossCommandState(attackState, "INITIATING SURGE..."));
            }

            if (ImGui::TreeNode("Wire Settings"))
            {
                // Slider untuk Scale (XYZ)
                ImGui::DragFloat3("Scale", &m_wireConfig.scale.x, 0.1f, 0.1f, 10.0f);
                ImGui::Text("Anchor Adjustment:");
                ImGui::DragFloat3("Pivot Offset", &m_wireConfig.pivotOffset.x, 0.01f);
                // Slider untuk Posisi/Gameplay
                ImGui::DragFloat("Target Spread", &m_wireConfig.targetSpread, 0.1f, 0.0f, 10.0f);
                ImGui::DragFloat("Speed", &m_wireConfig.speed, 0.5f, 1.0f, 100.0f);
                ImGui::DragFloat("Hit Radius", &m_wireConfig.hitRadius, 0.1f, 0.1f, 5.0f);

                // Tombol Reset Config (Opsional)
                if (ImGui::Button("Reset Defaults")) {
                    // URUTAN: Scale, PivotOffset, Speed, Spread, Radius
                    m_wireConfig = {
                        {100.f, 100.f, 100.f}, // Scale
                        {0.0f, 0.0f, -10.0f},   // Pivot Offset (YANG KURANG TADI)
                        25.0f,                 // Speed
                        2.0f,                  // Target Spread
                        1.5f                   // Hit Radius
                    };
                }

                ImGui::TreePop();
            }

            // Pentagon Config
            if (ImGui::DragFloat("Pentagon Scale", &m_pentagonConfig.scale, 1.0f, 1.0f, 500.0f)) {
                if (m_enemyManager) {
                    for (auto& e : m_enemyManager->GetEnemies())
                        if (e->GetType() == EnemyType::Pentagon)
                            e->SetScale({ m_pentagonConfig.scale, m_pentagonConfig.scale, m_pentagonConfig.scale });
                }
            }
            ImGui::DragFloat3("Pentagon Pos", &m_pentagonConfig.pos.x, 0.5f);

            ImGui::Separator();
            if (ImGui::TreeNode("Parts Transform")) {
                for (auto& part : m_parts) {
                    if (ImGui::TreeNode(part->name.c_str())) {
                        ImGui::DragFloat3("Pos", &part->position.x, 0.1f);
                        ImGui::DragFloat3("Rot", &part->rotation.x, 1.0f);
                        ImGui::DragFloat3("Scl", &part->scale.x, 0.1f);
                        ImGui::Checkbox("Float", &part->useFloating);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            ImGui::Checkbox("Debug BG", &m_debugForceBG);
            ImGui::EndTabItem();
        }

        // TAB 2: ANTENNA
        if (ImGui::BeginTabItem("Antenna Tool"))
        {
            auto* p = GetPart("antenna");
            if (p) {
                if (ImGui::Checkbox("Edit Mode (Stop Floating)", &m_debugShowAntenna)) p->useFloating = !m_debugShowAntenna;

                ImGui::DragFloat3("Hidden Pos", &m_antennaConfig.hiddenPos.x, 0.1f);
                if (ImGui::Button("Snap Hidden")) p->position = m_antennaConfig.hiddenPos;

                ImGui::DragFloat3("Show Pos", &m_antennaConfig.showPos.x, 0.1f);
                if (ImGui::Button("Snap Show")) p->position = m_antennaConfig.showPos;

                if (ImGui::DragFloat3("Rotation", &m_antennaConfig.rotation.x, 1.0f)) p->rotation = m_antennaConfig.rotation;
                if (ImGui::DragFloat3("Scale", &m_antennaConfig.scale.x, 0.1f)) p->scale = m_antennaConfig.scale;

                ImGui::DragFloat3("Spawn Source", &m_antennaConfig.spawnSrc.x, 0.1f);
                if (ImGui::Button("Snap Source")) p->position = m_antennaConfig.spawnSrc;
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Screen Calib"))
        {
            ImGui::Text("--- Monitor 1 (Main / Terminal 1) ---");
            // Geser slider ini untuk mengatur posisi Layar Utama
            ImGui::DragFloat3("M1 Offset", &m_screen1Config.offset.x, 0.001f);
            ImGui::DragFloat3("M1 Rot", &m_screen1Config.rotation.x, 0.5f);
            ImGui::DragFloat3("M1 Scale", &m_screen1Config.scale.x, 0.1f);

            ImGui::Separator();

            ImGui::Text("--- Monitor 2 (Side Left / Terminal 2) ---");
            // Geser slider ini untuk mengatur posisi Layar Samping (yang hilang)
            ImGui::DragFloat3("M2 Offset", &m_screen2Config.offset.x, 0.001f);
            ImGui::DragFloat3("M2 Rot", &m_screen2Config.rotation.x, 0.5f);
            ImGui::DragFloat3("M2 Scale", &m_screen2Config.scale.x, 0.1f);

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        ImGui::Separator();
        if (ImGui::TreeNode("Wire Settings (DEBUG)"))
        {
            // --------------------------------------------------------
            // 1. STATUS CHECKER
            // --------------------------------------------------------
            // Cek apakah model berhasil di-load?
            bool modelOK = (m_wireModel1 != nullptr);
            if (modelOK)
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Model Loaded");
            else
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "[ERR] Model NULL! Cek path file .glb");

            ImGui::Separator();

            // --------------------------------------------------------
            // 2. CONFIG NORMAL
            // --------------------------------------------------------
            ImGui::Text("Gameplay Config:");
            ImGui::DragFloat3("Scale", &m_wireConfig.scale.x, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Target Spread", &m_wireConfig.targetSpread, 0.1f);
            ImGui::DragFloat("Speed", &m_wireConfig.speed, 0.5f);
            ImGui::DragFloat("Hit Radius", &m_wireConfig.hitRadius, 0.1f);

            ImGui::Separator();

            // --------------------------------------------------------
            // 3. FORCE SPAWN (DIAM DI TEMPAT)
            // --------------------------------------------------------
            ImGui::Text("Debug Tools:");

            // Variabel static biar nilainya tersimpan selama game jalan
            static float debugSpawnPos[3] = { 0.0f, 0.0f, 0.0f };
            ImGui::DragFloat3("Spawn At", debugSpawnPos, 0.5f);

            if (ImGui::Button("FORCE SPAWN (STATIC)"))
            {
                // Cari slot kosong manual
                for (auto& w : m_wires)
                {
                    if (w.state == WireState::INACTIVE)
                    {
                        // Reset Data
                        w.id = 999; // ID Debug

                        // Set Posisi sesuai Input Slider
                        w.position = { debugSpawnPos[0], debugSpawnPos[1], debugSpawnPos[2] };
                        w.rotation = { 0, 0, 0 }; // Tegak lurus

                        // STATE PENTING: Gunakan WARNING (fase diam/nancap)
                        w.state = WireState::WARNING;

                        // Timer SANGAT LAMA biar gak hilang
                        w.timer = 9999.0f;

                        w.modelIndex = 0; // Pakai model 1
                        break; // Cukup spawn 1 aja
                    }
                }
            }

            // --------------------------------------------------------
            // 4. LIST ACTIVE WIRES (LIVE EDIT)
            // --------------------------------------------------------
            // Ini gunanya buat nyari wire yang "hilang" ada di koordinat mana
            if (ImGui::TreeNode("Active Wires List"))
            {
                bool found = false;
                for (int i = 0; i < m_wires.size(); ++i)
                {
                    auto& w = m_wires[i];
                    if (w.state != WireState::INACTIVE)
                    {
                        found = true;
                        ImGui::PushID(i);

                        std::string label = "Wire " + std::to_string(i) + " [" +
                            (w.state == WireState::TRAVELING ? "FLY" : "STUCK") + "]";

                        if (ImGui::TreeNode(label.c_str()))
                        {
                            // EDIT POSISI LANGSUNG
                            ImGui::DragFloat3("Pos", &w.position.x, 0.1f);
                            ImGui::DragFloat3("Rot", &w.rotation.x, 1.0f);

                            // TOMBOL KILL
                            if (ImGui::Button("Delete")) w.state = WireState::INACTIVE;

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                }
                if (!found) ImGui::TextDisabled("No active wires.");
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
}

void Boss::SpawnSingleWire(const DirectX::XMFLOAT3& playerPos)
{
    // 1. Cari Wire yang sedang INACTIVE
    ElectricWire* wire = nullptr;
    for (auto& w : m_wires) {
        if (w.state == WireState::INACTIVE) {
            wire = &w;
            break;
        }
    }
    if (!wire) return; // Pool penuh

    // 2. Tentukan Posisi Awal (Dari Monitor 1)
    wire->startPos = GetMonitorVisualPos();
    wire->position = wire->startPos;

    // SETUP DATA BARU DENGAN CONFIG
    wire->speed = m_wireConfig.speed;   // <--- Ganti hardcoded value
    wire->radius = m_wireConfig.hitRadius; // <--- Ganti hardcoded value

    // 3. Hitung Arah ke Player (Hanya X dan Z, Y diabaikan dulu biar nembak lurus ground)
    float dx = playerPos.x - wire->startPos.x;
    float dz = playerPos.z - wire->startPos.z;

    // Normalisasi arah
    float dist = sqrt(dx * dx + dz * dz);
    wire->direction = { dx / dist, 0.0f, dz / dist }; // Y 0 biar menyusur tanah atau menukik nanti

    // Hitung Rotasi (Y-Axis facing player)
    float yawRad = atan2f(dx, dz);
    float yawDeg = XMConvertToDegrees(yawRad);

    // UBAH BARIS INI:
    // X = 90 (Nungging/Tegak), Y = Yaw (Arah Player), Z = 90 (Rotasi Sumbu)
    wire->rotation = { 90.0f, yawDeg, 90.0f };

    // 4. Setup State
    wire->state = WireState::TRAVELING;
    wire->timer = 0.0f;
    wire->modelIndex = rand() % 2; // Random model 1 atau 2

    // [OPTIONAL] Offset sedikit targetnya biar tidak 100% aimbot
    // float offset = (rand() % 100 - 50) / 10.0f; 
}

void Boss::TriggerWireAttack()
{
    if (!m_player) return;

    XMFLOAT3 pPos = m_player->GetPosition();

    // Gunakan m_wireConfig.targetSpread
    float spread = m_wireConfig.targetSpread;

    // Tembak 2 kabel (Kanan & Kiri)
    SpawnSingleWire({ pPos.x + spread, 0, pPos.z });
    SpawnSingleWire({ pPos.x - spread, 0, pPos.z });
}

void Boss::UpdateWires(float dt)
{
    for (auto& w : m_wires)
    {
        if (w.state == WireState::INACTIVE) continue;

        if (w.state == WireState::TRAVELING)
        {
            // Gerakkan kabel
            w.position.x += w.direction.x * w.speed * dt;
            w.position.z += w.direction.z * w.speed * dt;

            // Logika "Nancep":
            // Bisa berdasarkan jarak tempuh, atau jika sudah menyentuh lantai (Y <= 0)
            // Disini kita asumsi dia turun dari ketinggian monitor ke tanah
            // Kita simulasikan gerak parabola simpel untuk Y

            float groundLevel = 0.0f;
            if (w.position.y > groundLevel) {
                w.position.y -= 15.0f * dt; // Gravity speed
            }
            else {
                w.position.y = groundLevel;
                // Sampai di tanah -> Pindah ke WARNING
                w.state = WireState::WARNING;
                w.timer = 2.0f; // 2 Detik menunggu
            }
        }
        else if (w.state == WireState::WARNING)
        {
            w.timer -= dt;
            if (w.timer <= 0.0f) {
                w.state = WireState::ACTIVE; // MULAI NYETRUM
                w.timer = 3.0f; // Nyetrum selama 3 detik
            }
        }
        else if (w.state == WireState::ACTIVE)
        {
            // === LOGIKA COLLISION ===
            if (m_player) {
                XMFLOAT3 pPos = m_player->GetPosition();

                // Hitung jarak Player ke Garis Kabel
                // Kabel dianggap garis lurus dari w.position mundur ke belakang sesuai rotasi
                // ATAU simplenya: Gunakan radius di titik jatuhnya kabel

                float dx = pPos.x - w.position.x;
                float dz = pPos.z - w.position.z;
                float distSq = dx * dx + dz * dz;

                // Jika kena area setrum
                if (distSq < (w.radius * w.radius)) {
                    // Panggil fungsi damage player
                    // m_player->TakeDamage(10); 
                    // WindowShatterManager::Instance().TriggerExplosion(pPos, 2);
                    OutputDebugStringA("ZAP! Player tersengat listrik!\n");
                }
            }

            w.timer -= dt;
            if (w.timer <= 0.0f) w.state = WireState::FADING;
        }
        else if (w.state == WireState::FADING)
        {
            // Animasi mengecil atau tenggelam
            w.position.y -= 5.0f * dt;
            if (w.position.y < -5.0f) w.state = WireState::INACTIVE;
        }
    }
}

void Boss::RenderWires(ModelRenderer* renderer)
{
    for (const auto& w : m_wires)
    {
        if (w.state == WireState::INACTIVE) continue;

        // Pilih model
        auto model = (w.modelIndex == 0) ? m_wireModel1 : m_wireModel2;
        if (!model) continue;

        // Visual Feedback Warna
        XMFLOAT4 color = { 1,1,1,1 };
        if (w.state == WireState::WARNING) {
            // Kedap kedip Merah
            float flash = sinf(GetTickCount64() * 0.01f); // Cepat
            color = (flash > 0) ? XMFLOAT4(1, 0, 0, 1) : XMFLOAT4(0.5f, 0, 0, 1);
        }
        else if (w.state == WireState::ACTIVE) {
            // Biru Listrik / Kuning
            float flash = sinf(GetTickCount64() * 0.05f);
            color = (flash > 0) ? XMFLOAT4(0, 1, 1, 1) : XMFLOAT4(1, 1, 0, 1);
        }

        // === MATRIX CALCULATION ===

                // 1. Scale
        XMMATRIX S = XMMatrixScaling(
            m_wireConfig.scale.x,
            m_wireConfig.scale.y,
            m_wireConfig.scale.z
        );

        // 2. Pivot Offset (BARU)
        // Geser model secara lokal agar "Ujung Atas" berada di titik 0,0,0
        XMMATRIX T_Pivot = XMMatrixTranslation(
            m_wireConfig.pivotOffset.x,
            m_wireConfig.pivotOffset.y,
            m_wireConfig.pivotOffset.z
        );

        // 3. Rotation
        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(w.rotation.x),
            XMConvertToRadians(w.rotation.y),
            XMConvertToRadians(w.rotation.z)
        );

        // 4. World Position
        XMMATRIX T = XMMatrixTranslation(w.position.x, w.position.y, w.position.z);

        // GABUNGKAN: Scale -> Geser Pivot -> Putar -> Pindah ke Lokasi Asli
        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, S * T_Pivot * R * T);

        renderer->Draw(ShaderId::Lambert, model, color, world);
    }
}