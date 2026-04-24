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
        auto MakeMaterialUnique = [&](std::shared_ptr<Model> model)
            {
                if (!model) return;

                auto& mutableMeshes = const_cast<std::vector<Model::Mesh>&>(model->GetMeshes());

                for (auto& mesh : mutableMeshes)
                {
                    if (mesh.material)
                    {
                        auto uniqueMat = std::make_shared<Model::Material>(*mesh.material);
                        m_materialCache.push_back(uniqueMat);
                        mesh.material = uniqueMat.get();
                    }
                }
            };

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

    // =========================================================
    // CONCURRENT SPAWN: Spawn minion sambil state lain jalan
    // =========================================================
    if (m_concurrentSpawnConfig.enabled && IsConcurrentSpawnEligibleState() && m_enemyManager && m_player)
    {
        m_concurrentSpawnTimer += dt;

        if (m_concurrentSpawnTimer >= m_concurrentSpawnConfig.activateDelay)
        {
            // Setelah activateDelay terlewat, spawn setiap `interval` detik
            float effectiveInterval = m_concurrentSpawnConfig.interval;

            if (m_concurrentSpawnTimer >= (m_concurrentSpawnConfig.activateDelay + effectiveInterval))
            {
                m_concurrentSpawnTimer = m_concurrentSpawnConfig.activateDelay; // Reset ke batas (bukan 0), biar interval konsisten

                // --- Tentukan posisi spawn ---
                // Pilih sisi acak (kiri / kanan) dan Z acak di area tengah layar
                float side = (rand() % 2 == 0) ? 1.0f : -1.0f;
                float xPos = side * (8.0f + (rand() % 40) / 10.0f); // 8.0 - 12.0, kiri atau kanan
                float zPos = -6.0f + (rand() % 120) / 10.0f;        // -6.0 sampai +6.0

                XMFLOAT3 spawnPos = { xPos, 0.0f, zPos };

                // --- Safety check: jangan spawn terlalu dekat player ---
                XMFLOAT3 pPos = m_player->GetPosition();
                float dx = spawnPos.x - pPos.x;
                float dz = spawnPos.z - pPos.z;
                float distSq = dx * dx + dz * dz;
                float safeDistSq = m_concurrentSpawnConfig.safeDistance * m_concurrentSpawnConfig.safeDistance;

                if (distSq < safeDistSq)
                {
                    // Lempar ke sisi sebelah
                    spawnPos.x = -spawnPos.x;
                }

                // --- Spawn ---
                EnemySpawnConfig config;
                config.Position = spawnPos;
                config.Color = { 1.f, 0.f, 0.f, 1.f };
                config.Type = EnemyType::Paddle;
                config.AttackBehavior = AttackType::Tracking;
                config.Direction = MoveDir::None;

                m_enemyManager->SpawnEnemy(config);
                AddTerminalLog("CONCURRENT: UNIT_SPAWNED");
            }
        }
    }
    else
    {
        // Reset timer kalau state tidak eligible (misal balik ke Idle / SpawnEnemy)
        m_concurrentSpawnTimer = 0.0f;
    }
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

            renderer->Draw(ShaderId::Basic, screen, DirectX::XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), world);
        };


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
void Boss::TriggerSpawnEnemy()
{
    ChangeState(new BossCommandState(new BossSpawnEnemyState(), "INSTANCE: PADDLE_UNIT"));
}
void Boss::TriggerLockPlayer() { ChangeState(new BossCommandState(new BossLockPlayerState(), "INITIATE LOCK")); }
void Boss::TriggerSpawnPentagon() { ChangeState(new BossCommandState(new BossSpawnPentagonState(), "DEPLOYING HEAVY UNIT")); }

// [BARU] Trigger untuk Download Attack State
void Boss::TriggerDownloadAttack()
{
    ChangeState(new BossCommandState(new BossDownloadAttackState(), "DOWNLOADING..."));
}

// [BARU] Trigger untuk Wire Attack State
// Nama berbeda dari TriggerWireAttack() yang sudah ada (itu mechanic-level: spawn wire).
// Ini adalah entry point ke full Wire Attack STATE (lengkap dengan typing animation).
void Boss::TriggerWireAttackState()
{
    ChangeState(new BossCommandState(new BossWireAttackState(), "INITIATING SURGE..."));
}

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
            if (ImGui::Button("SPAWN ENEMY"))
            {
                ChangeState(new BossCommandState(new BossSpawnEnemyState(), "INSTANCE: PADDLE_UNIT"));
            }
            if (ImGui::Button("LOCK PLAYER")) TriggerLockPlayer(); ImGui::SameLine();
            if (ImGui::Button("DOWNLOAD")) ChangeState(new BossCommandState(new BossDownloadAttackState(), "DOWNLOADING..."));

            ImGui::Separator();
            if (ImGui::Button("SPAWN PENTAGON")) TriggerSpawnPentagon(); ImGui::SameLine();
            if (ImGui::Button("WIRE ATTACK"))
            {
                BossState* attackState = new BossWireAttackState();
                ChangeState(new BossCommandState(attackState, "INITIATING SURGE..."));
            }

            if (ImGui::TreeNode("Wire Settings"))
            {
                ImGui::DragFloat3("Scale", &m_wireConfig.scale.x, 0.1f, 0.1f, 10.0f);
                ImGui::Text("Anchor Adjustment:");
                ImGui::DragFloat3("Pivot Offset", &m_wireConfig.pivotOffset.x, 0.01f);
                ImGui::DragFloat("Target Spread", &m_wireConfig.targetSpread, 0.1f, 0.0f, 10.0f);
                ImGui::DragFloat("Speed", &m_wireConfig.speed, 0.5f, 1.0f, 100.0f);
                ImGui::DragFloat("Hit Radius", &m_wireConfig.hitRadius, 0.1f, 0.1f, 5.0f);

                if (ImGui::Button("Reset Defaults")) {
                    m_wireConfig = {
                        {100.f, 100.f, 100.f},
                        {0.0f, 0.0f, -10.0f},
                        25.0f,
                        2.0f,
                        1.5f
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
            ImGui::DragFloat3("M1 Offset", &m_screen1Config.offset.x, 0.001f);
            ImGui::DragFloat3("M1 Rot", &m_screen1Config.rotation.x, 0.5f);
            ImGui::DragFloat3("M1 Scale", &m_screen1Config.scale.x, 0.1f);

            ImGui::Separator();

            ImGui::Text("--- Monitor 2 (Side Left / Terminal 2) ---");
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

            static float debugSpawnPos[3] = { 0.0f, 0.0f, 0.0f };
            ImGui::DragFloat3("Spawn At", debugSpawnPos, 0.5f);

            if (ImGui::Button("FORCE SPAWN (STATIC)"))
            {
                for (auto& w : m_wires)
                {
                    if (w.state == WireState::INACTIVE)
                    {
                        w.id = 999;
                        w.position = { debugSpawnPos[0], debugSpawnPos[1], debugSpawnPos[2] };
                        w.rotation = { 0, 0, 0 };
                        w.state = WireState::WARNING;
                        w.timer = 9999.0f;
                        w.modelIndex = 0;
                        break;
                    }
                }
            }

            // --------------------------------------------------------
            // 4. LIST ACTIVE WIRES (LIVE EDIT)
            // --------------------------------------------------------
            if (ImGui::TreeNode("Active Wires List"))
            {
                bool found = false;
                for (int i = 0; i < (int)m_wires.size(); ++i)
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
                            ImGui::DragFloat3("Pos", &w.position.x, 0.1f);
                            ImGui::DragFloat3("Rot", &w.rotation.x, 1.0f);

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

bool Boss::IsConcurrentSpawnEligibleState() const
{
    // Concurrent spawn hanya aktif saat state yang "memakan waktu" 
    // yaitu state di mana player biasanya punya breathing room.
    return m_stateMachine.IsState("Lock Player")
        || m_stateMachine.IsState("Download Attack")
        || m_stateMachine.IsState("Wire Attack");
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
    wire->speed = m_wireConfig.speed;
    wire->radius = m_wireConfig.hitRadius;

    // 3. Hitung Arah ke Player (Hanya X dan Z, Y diabaikan dulu biar nembak lurus ground)
    float dx = playerPos.x - wire->startPos.x;
    float dz = playerPos.z - wire->startPos.z;

    // Normalisasi arah
    float dist = sqrt(dx * dx + dz * dz);
    wire->direction = { dx / dist, 0.0f, dz / dist };

    // Hitung Rotasi (Y-Axis facing player)
    float yawRad = atan2f(dx, dz);
    float yawDeg = XMConvertToDegrees(yawRad);

    wire->rotation = { 90.0f, yawDeg, 90.0f };

    // 4. Setup State
    wire->state = WireState::TRAVELING;
    wire->timer = 0.0f;
    wire->modelIndex = rand() % 2; // Random model 1 atau 2
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

            float groundLevel = 0.0f;
            if (w.position.y > groundLevel) {
                w.position.y -= 15.0f * dt; // Gravity speed
            }
            else {
                w.position.y = groundLevel;
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

                float dx = pPos.x - w.position.x;
                float dz = pPos.z - w.position.z;
                float distSq = dx * dx + dz * dz;

                if (distSq < (w.radius * w.radius)) {
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

        auto model = (w.modelIndex == 0) ? m_wireModel1 : m_wireModel2;
        if (!model) continue;

        // =========================================================
        // LOGIKA WARNA (VISUAL EFFECT)
        // =========================================================
        XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Default

        if (w.state == WireState::WARNING)
        {
            float maxTime = 2.0f;
            float t = 1.0f - (w.timer / maxTime);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            float boost = 1.0f + (t * 15.0f);

            color.x = boost;
            color.y = 1.0f - t;
            color.z = 1.0f - t;
        }
        else if (w.state == WireState::ACTIVE)
        {
            int flicker = rand() % 3;

            if (flicker == 0)
                color = { 2.0f, 8.0f, 20.0f, 1.0f };
            else if (flicker == 1)
                color = { 20.0f, 20.0f, 20.0f, 1.0f };
            else
                color = { 0.0f, 2.0f, 10.0f, 1.0f };
        }

        // =========================================================
        // TRANSFORM MATRIX
        // =========================================================

        XMMATRIX S = XMMatrixScaling(
            m_wireConfig.scale.x,
            m_wireConfig.scale.y,
            m_wireConfig.scale.z
        );

        XMMATRIX T_Pivot = XMMatrixTranslation(
            m_wireConfig.pivotOffset.x,
            m_wireConfig.pivotOffset.y,
            m_wireConfig.pivotOffset.z
        );

        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(w.rotation.x),
            XMConvertToRadians(w.rotation.y),
            XMConvertToRadians(w.rotation.z)
        );

        float shakeX = 0.0f;
        float shakeZ = 0.0f;

        if (w.state == WireState::ACTIVE)
        {
            float intensity = 0.15f;
            shakeX = ((rand() % 100) / 50.0f - 1.0f) * intensity;
            shakeZ = ((rand() % 100) / 50.0f - 1.0f) * intensity;
        }

        XMMATRIX T = XMMatrixTranslation(
            w.position.x + shakeX,
            w.position.y,
            w.position.z + shakeZ
        );

        XMFLOAT4X4 world;
        XMStoreFloat4x4(&world, S * T_Pivot * R * T);

        renderer->Draw(ShaderId::Lambert, model, color, world);
    }
}