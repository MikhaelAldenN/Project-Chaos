#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

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
            if (ImGui::Button("SPAWN PENTAGON")) TriggerSpawnPentagon();

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
    }
}