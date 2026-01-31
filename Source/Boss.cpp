#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>

using namespace DirectX;

// =========================================================
// BOSS PART IMPLEMENTATION
// =========================================================

void BossPart::Update(float dt)
{
    if (!model) return;

    // Floating Animation Calculation
    float offsetX = 0.0f, offsetY = 0.0f, offsetZ = 0.0f;
    if (useFloating)
    {
        timer += dt;
        float wave = sin(timer * floatSpeed) * floatIntensity;
        offsetX = wave * floatAxis.x;
        offsetY = wave * floatAxis.y;
        offsetZ = wave * floatAxis.z;
    }

    visualPosition = { position.x + offsetX, position.y + offsetY, position.z + offsetZ };

    // Update Model Matrix
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );
    XMMATRIX T = XMMatrixTranslation(visualPosition.x, visualPosition.y, visualPosition.z);

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, S * R * T);
    model->UpdateTransform(worldFloat);
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
    InitializeDefaultParts();

    auto device = Graphics::Instance().GetDevice();

    // 1. Init Monitor 2 (Log Terminal)
    m_terminal.Initialize(device, 512, 512);
    m_terminal.AddLog("SYSTEM BOOT...");
    m_terminal.AddLog("WAITING FOR USER...");

    // 2. Init Monitor 1 (Command/Cursor)
    m_terminal1.Initialize(device, 512, 512);
    // [FIXED] Removed AddLog calls here because TerminalMonitor1 no longer supports logs.
    // Instead, it defaults to IDLE state (blinking cursor).

    m_bgChainSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Beyond/Sprite_BackChain.png");

    // 3. Load Screen Models (Planes)
    try {
        // Shared instance cannot be used because they need different materials/textures
        m_screenQuad = std::make_shared<Model>(device, "Data/Model/Primitive/Plane.glb"); // Mon 2
        m_screenQuad1 = std::make_shared<Model>(device, "Data/Model/Primitive/Plane.glb"); // Mon 1
    }
    catch (...) {
        OutputDebugStringA("[BOSS] WARNING: Plane.glb for screens not found!\n");
        m_screenQuad = nullptr;
        m_screenQuad1 = nullptr;
    }

    try {
        m_fileModel = std::make_shared<Model>(device, "Data/Model/Character/Test_mdl_File.glb");
    }
    catch (...) {}

    // Reserve vector data (bukan model)
    m_fileProjectiles.reserve(100);

    // 4. Start AI
    m_stateMachine.Initialize(new BossIntroState(), this);
}

Boss::~Boss() {}

// =========================================================
// CORE LOOP
// =========================================================

void Boss::Update(float dt)
{
    // Update all body parts
    float safeDt = dt;
    if (safeDt > 0.05f) safeDt = 0.05f;

    // Gunakan safeDt untuk update parts & AI
    for (auto& part : m_parts)
    {
        part->Update(safeDt); // <-- Ganti dt jadi safeDt
    }

    // Update Terminal Logic (Animations/Scrolling)
    m_terminal.Update(dt);
    m_terminal1.Update(dt);

    // Update AI State
    m_stateMachine.Update(this, safeDt);

    // =========================================================
    // [BARU] UPDATE BACKGROUND OPACITY ANIMATION
    // =========================================================

    // 1. Cek apakah SEHARUSNYA muncul? (Locked Phase atau Debug)
    bool shouldBeVisible = m_terminal1.IsSystemLocked() || m_debugForceBG;

    // 2. State Transition Logic
    if (shouldBeVisible)
    {
        // Jika sebelumnya mati/keluar, kita RESET ke fase Masuk
        if (m_bgState == BgAnimState::HIDDEN || m_bgState == BgAnimState::EXITING)
        {
            m_bgState = BgAnimState::ENTERING;
            m_bgAnimTimer = 0.0f;
        }
    }
    else
    {
        // Jika sebelumnya nyala, kita ganti ke fase Keluar
        if (m_bgState == BgAnimState::ENTERING || m_bgState == BgAnimState::ACTIVE)
        {
            m_bgState = BgAnimState::EXITING;
            // Tidak perlu reset timer, kita pakai logic pengurangan alpha langsung
        }
    }

    // 3. Animation Process Logic
    switch (m_bgState)
    {
    case BgAnimState::HIDDEN:
        m_bgAlpha = 0.0f;
        break;

    case BgAnimState::ENTERING:
        m_bgAnimTimer += dt;

        // Fase A: Naik Cepat (0.0 -> 1.0) - "The Flash"
        if (m_bgAnimTimer <= m_bgFlashDuration)
        {
            float t = m_bgAnimTimer / m_bgFlashDuration;
            // Lerp 0 -> 1
            m_bgAlpha = t;
        }
        // Fase B: Turun Stabil (1.0 -> 0.7) - "Settle Down"
        else if (m_bgAnimTimer <= (m_bgFlashDuration + m_bgSettleDuration))
        {
            float timeInPhase = m_bgAnimTimer - m_bgFlashDuration;
            float t = timeInPhase / m_bgSettleDuration;

            // Lerp 1.0 -> 0.7
            m_bgAlpha = 1.0f + (m_bgBaseOpacity - 1.0f) * t;
        }
        else
        {
            // Selesai animasi masuk
            m_bgAlpha = m_bgBaseOpacity;
            m_bgState = BgAnimState::ACTIVE;
        }
        break;

    case BgAnimState::ACTIVE:
        // Kunci di opacity default
        m_bgAlpha = m_bgBaseOpacity;
        break;

    case BgAnimState::EXITING:
        // Kurangi alpha pelan-pelan sampai 0
        m_bgAlpha -= m_bgFadeOutSpeed * dt;
        if (m_bgAlpha <= 0.0f)
        {
            m_bgAlpha = 0.0f;
            m_bgState = BgAnimState::HIDDEN;
        }
        break;
    }
}


void Boss::Render(ModelRenderer* renderer, Camera* camera)
{
    auto context = Graphics::Instance().GetDeviceContext();
    auto font = ResourceManager::Instance().GetFont("VGA_FONT");

    // 1. Update Render-To-Texture (RTT) for terminals
    if (font) {
        m_terminal.RenderToTexture(context, font);  // Monitor 2 (Logs)
        m_terminal1.RenderToTexture(context, font); // Monitor 1 (Command)
    }

    // Chain
    if (m_bgAlpha > 0.0f && m_bgChainSprite && camera)
    {
        auto context = Graphics::Instance().GetDeviceContext();
        auto rs = Graphics::Instance().GetRenderState();

        context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);

        // [UPDATE] Gunakan m_bgAlpha di parameter terakhir (a)
        m_bgChainSprite->Render(context,
            camera,
            m_bgChainPos.x, m_bgChainPos.y, m_bgChainPos.z,
            m_bgChainSize.x, m_bgChainSize.y,
            DirectX::XMConvertToRadians(m_bgChainRotation.x),
            DirectX::XMConvertToRadians(m_bgChainRotation.y),
            DirectX::XMConvertToRadians(m_bgChainRotation.z),
            1.0f, 1.0f, 1.0f, m_bgAlpha // <--- PAKAI VARIABEL INI
        );

        context->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
        context->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);
    }

    // 2. Render 3D Body Parts
    for (auto& part : m_parts) {

        // [CULLING CHECK]
        if (camera)
        {
            // [BARU] Tambahkan offset ke posisi visual
            // Karena visualPosition sudah termasuk floating animation,
            // offset ini akan ikut bergerak naik turun (Correct).
            float checkX = part->visualPosition.x + part->cullOffset.x;
            float checkY = part->visualPosition.y + part->cullOffset.y;
            float checkZ = part->visualPosition.z + part->cullOffset.z;

            if (!camera->CheckSphere(checkX, checkY, checkZ, part->cullRadius))
            {
                continue; // SKIP
            }
        }

        part->Render(renderer);
    }

    // 3. Render Monitor 2 Screen (Overlay)
    if (m_screenQuad && HasPart("monitor2"))
    {
        BossPart* monitor = GetPart("monitor2");

        // Calculate Transform Matrix (Parent * Local Offset)
        XMMATRIX matMonitor = XMMatrixScaling(monitor->scale.x, monitor->scale.y, monitor->scale.z) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(monitor->rotation.x), XMConvertToRadians(monitor->rotation.y), XMConvertToRadians(monitor->rotation.z)) *
            XMMatrixTranslation(monitor->visualPosition.x, monitor->visualPosition.y, monitor->visualPosition.z);

        XMMATRIX matScreen = XMMatrixScaling(m_screenScale.x, m_screenScale.y, m_screenScale.z) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_screenRotation.x), XMConvertToRadians(m_screenRotation.y), XMConvertToRadians(m_screenRotation.z)) *
            XMMatrixTranslation(m_screenOffset.x, m_screenOffset.y, m_screenOffset.z);

        XMFLOAT4X4 worldFinal;
        XMStoreFloat4x4(&worldFinal, matScreen * matMonitor);
        m_screenQuad->UpdateTransform(worldFinal);

        // Apply Texture
        ID3D11ShaderResourceView* tex = m_terminal.GetTexture();
        if (tex) {
            for (const auto& mesh : m_screenQuad->GetMeshes()) {
                if (mesh.material) mesh.material->baseMap = tex;
            }
        }
        renderer->Draw(ShaderId::Basic, m_screenQuad, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // 4. Render Monitor 1 Screen (Overlay)
    if (m_screenQuad1 && HasPart("monitor1"))
    {
        BossPart* monitor1 = GetPart("monitor1");

        // Calculate Transform Matrix
        XMMATRIX matMonitor1 = XMMatrixScaling(monitor1->scale.x, monitor1->scale.y, monitor1->scale.z) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(monitor1->rotation.x), XMConvertToRadians(monitor1->rotation.y), XMConvertToRadians(monitor1->rotation.z)) *
            XMMatrixTranslation(monitor1->visualPosition.x, monitor1->visualPosition.y, monitor1->visualPosition.z);

        XMMATRIX matScreen1 = XMMatrixScaling(m_screen1Scale.x, m_screen1Scale.y, m_screen1Scale.z) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_screen1Rotation.x), XMConvertToRadians(m_screen1Rotation.y), XMConvertToRadians(m_screen1Rotation.z)) *
            XMMatrixTranslation(m_screen1Offset.x, m_screen1Offset.y, m_screen1Offset.z);

        XMFLOAT4X4 worldFinal1;
        XMStoreFloat4x4(&worldFinal1, matScreen1 * matMonitor1);
        m_screenQuad1->UpdateTransform(worldFinal1);

        // Apply Texture
        ID3D11ShaderResourceView* tex1 = m_terminal1.GetTexture();
        if (tex1) {
            for (const auto& mesh : m_screenQuad1->GetMeshes()) {
                if (mesh.material) mesh.material->baseMap = tex1;
            }
        }
        renderer->Draw(ShaderId::Basic, m_screenQuad1, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // [BARU] RENDER FILE PROJECTILES
    if (m_fileModel)
    {
        const auto& projectiles = m_fileProjectiles;

        for (const auto& p : projectiles)
        {
            if (!p.active) continue;

            // 1. Hitung Posisi
            float displayScale = 150.0f; // Sesuaikan skala
            XMMATRIX S = XMMatrixScaling(displayScale, displayScale, displayScale);
            XMMATRIX R = XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(p.rotation.x),
                XMConvertToRadians(p.rotation.y),
                XMConvertToRadians(p.rotation.z));
            XMMATRIX T = XMMatrixTranslation(p.position.x, p.position.y, p.position.z);

            XMFLOAT4X4 world;
            XMStoreFloat4x4(&world, S * R * T);

            // 2. DRAW DENGAN MATRIX (Parameter ke-4)
            // Ini akan mengirim posisi spesifik ini ke antrian render.
            // Tidak perlu lagi memanggil m_fileModel->(Update)Transform(world) di sini!

            renderer->Draw(ShaderId::Lambert, m_fileModel, { 0.0f, 1.0f, 1.0f, 1.0f }, world);
        }
    }
}

// =========================================================
// AI COMMANDS & STATE MANAGEMENT
// =========================================================

void Boss::ChangeState(BossState* newState)
{
    m_stateMachine.ChangeState(this, newState);
}

void Boss::TriggerIdle()
{
    ChangeState(new BossIdleState());
}

void Boss::TriggerSpawnEnemy()
{
    // Wrapper: Type command first, then spawn
    ChangeState(new BossCommandState(new BossSpawnEnemyState(), "DEPLOY MINIONS"));
}

void Boss::TriggerLockPlayer()
{
    // Wrapper: Type command first, then lock
    ChangeState(new BossCommandState(new BossLockPlayerState(), "INITIATE LOCK"));
}

void Boss::AddTerminalLog(const std::string& msg)
{
    m_terminal.AddLog(msg);
}

// =========================================================
// PARTS & INITIALIZATION
// =========================================================

void Boss::InitializeDefaultParts()
{
    std::vector<BossPartConfig> configs = {
        { "monitor1", "Data/Model/Character/TEST_mdl_CRTMonitor.glb", { 0.0f, 0.6f, 6.5f }, { 65.0f, 0.0f, 0.0f }, { 10.0f, 10.0f, 10.0f }, true, 2.0f, 0.2f, { 0.0f, 1.0f, 0.0f } },
        { "cpu",      "Data/Model/Character/TEST_mdl_CPU.glb",        { 2.0f, -1.6f, 1.9f }, { -86.0f, 0.0f, 176.0f }, { 150.0f, 150.0f, 150.0f }, true, 1.5f, 0.2f, { 1.0f, 1.0f, 0.0f }, 4.5f, {0.0f, 10.0f, 0.0f} },
        { "monitor2", "Data/Model/Character/Test_mdl_CRTMonitor2.glb", { 5.5f, 0.0f, 4.2f }, { -44.0f, 147.0f, 31.0f }, { 100.0f, 100.0f, 100.0f }, true, 1.0f, 0.05f, { 1.0f, 0.0f, -1.0f } },
        { "monitor3", "Data/Model/Character/TEST_mdl_CRTMonitor3.glb", { 5.0f, 2.3f, 7.5f }, { 110.0f, 123.0f, 128.0f }, { 100.0f, 100.0f, 100.0f }, true, 1.0f, 0.25f, { 0.5f, 0.5f, 0.5f } },
        { "cable1",   "Data/Model/Character/TEST_mdl_Cable1.glb",      { -8.6f, 0.0f, 13.9f }, { 90.0f, 0.0f, 0.0f }, { 10.0f, 10.0f, 10.0f }, true, 1.0f, 1.0f, { 1.0f, -1.0f, 0.0f } },
        { "cable2",   "Data/Model/Character/TEST_mdl_Cable1.glb",      { 6.3f, 5.5f, 12.9f }, { 90.0f, 0.0f, 0.0f }, { 8.0f, 8.0f, 8.0f }, true, 0.5f, 0.5f, { -1.0f, 1.0f, 0.0f } },
        { "cable3",   "Data/Model/Character/TEST_mdl_Cable3.glb",      { 2.0f, 0.7f, 3.0f }, { 156.0f, -16.0f, -133.0f }, { 0.03f, 0.03f, 0.03f }, true, 2.0f, 0.2f, { 0.0f, 1.0f, 0.0f } },
        { "platform1","Data/Model/Character/TEST_mdl_Platform.glb",    { -9.6f, 11.2f, 8.7f }, { -62.0f, 138.0f, 48.0f }, { 2.0f, 2.0f, 2.0f }, true, 1.0f, 0.2f, { 0.0f, 1.0f, 0.0f } },
        { "platform2","Data/Model/Character/TEST_mdl_Platform.glb",    { 8.3f, 11.2f, 9.2f }, { 43.0f, 60.0f, 59.0f }, { 2.0f, 2.0f, 2.0f }, true, 1.0f, 0.2f, { 0.0f, 1.0f, 0.0f } },
        { "antenna", "Data/Model/Character/TEST_mdl_Antenna.glb",
          m_antennaHiddenPos, m_antennaRotation, m_antennaScale, false } 
    };

    for (const auto& config : configs)
    {
        AddPart(config);
    }
}

bool Boss::AddPart(const BossPartConfig& config)
{
    auto device = Graphics::Instance().GetDevice();
    auto newPart = std::make_unique<BossPart>();
    newPart->name = config.name;

    try {
        newPart->model = std::make_shared<Model>(device, config.modelPath.c_str());
        // printf("Loaded Boss Part: %s\n", config.name.c_str());
    }
    catch (const std::exception& e) {
        char msg[512];
        sprintf_s(msg, "FAILED to load Boss Part '%s': %s\n", config.name.c_str(), e.what());
        OutputDebugStringA(msg);
        return false;
    }

    newPart->position = config.position;
    newPart->rotation = config.rotation;
    newPart->scale = config.scale;
    newPart->useFloating = config.useFloating;
    newPart->floatSpeed = config.floatSpeed;
    newPart->floatIntensity = config.floatIntensity;
    newPart->floatAxis = config.floatAxis;
    newPart->cullRadius = config.cullRadius;
    newPart->cullOffset = config.cullOffset;

    m_partLookup[config.name] = newPart.get();
    m_parts.push_back(std::move(newPart));

    return true;
}

BossPart* Boss::GetPart(const std::string& name)
{
    auto it = m_partLookup.find(name);
    return (it != m_partLookup.end()) ? it->second : nullptr;
}

DirectX::XMFLOAT3 Boss::GetPartVisualPos(const std::string& name) const
{
    auto it = m_partLookup.find(name);
    return (it != m_partLookup.end()) ? it->second->visualPosition : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
}

bool Boss::HasPart(const std::string& name) const
{
    return m_partLookup.find(name) != m_partLookup.end();
}

std::vector<std::string> Boss::GetPartNames() const
{
    std::vector<std::string> names;
    names.reserve(m_parts.size());
    for (const auto& part : m_parts) names.push_back(part->name);
    return names;
}

void Boss::SpawnFileProjectile(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& targetPos)
{
    int activeCount = 0;
    for (const auto& p : m_fileProjectiles) if (p.active) activeCount++;

    if (m_fileProjectiles.size() >= 100)
    {
        // Opsional: Cari slot yang tidak aktif untuk direcycle (Object Pooling sederhana)
        for (auto& p : m_fileProjectiles)
        {
            if (!p.active)
            {
                p.active = true;
                p.position = startPos;
                p.targetPos = targetPos;
                p.rotation = { (float)(rand() % 360), (float)(rand() % 360), (float)(rand() % 360) };
                p.speed = 25.0f;
                return; // Selesai reuse, keluar fungsi
            }
        }
        return; // Kalau penuh dan semua aktif, batalkan spawn (limit tercapai)
    }

    FileProjectile p;
    p.active = true;
    p.id = m_projectileIdCounter++;
    p.position = startPos;
    p.targetPos = targetPos;
    p.rotation = { (float)(rand() % 360), (float)(rand() % 360), (float)(rand() % 360) };
    p.speed = 25.0f; // Kecepatan file terbang

    m_fileProjectiles.push_back(p);
}

void Boss::ClearProjectiles()
{
    m_fileProjectiles.clear();
}

// =========================================================
// DEBUG GUI
// =========================================================

void Boss::DrawDebugGUI()
{
    // Container Utama
    if (ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Mulai Sistem Tab
        if (ImGui::BeginTabBar("BossManagerTabs"))
        {
            // =========================================================
            // TAB 1: GENERAL CONTROLS (Main Boss Stuff)
            // =========================================================
            if (ImGui::BeginTabItem("General"))
            {
                ImGui::Spacing();
                ImGui::TextColored({ 1,1,0,1 }, "=== SYSTEM STATUS ===");

                bool mon2Ready = m_screenQuad && HasPart("monitor2");
                bool mon1Ready = m_screenQuad1 && HasPart("monitor1");

                ImGui::TextColored(mon2Ready ? ImVec4{ 0,1,0,1 } : ImVec4{ 1,0,0,1 }, "Monitor 2: %s", mon2Ready ? "ACTIVE" : "INACTIVE");
                ImGui::TextColored(mon1Ready ? ImVec4{ 0,1,0,1 } : ImVec4{ 1,0,0,1 }, "Monitor 1: %s", mon1Ready ? "ACTIVE" : "INACTIVE");

                // Terminal Sub-GUIs (Mereka punya header sendiri, akan dirender di dalam tab ini)
                ImGui::Separator();
                m_terminal.DrawGUI();
                ImGui::Separator();
                m_terminal1.DrawGUI();

                // AI Controls
                ImGui::Separator();
                ImGui::Text("AI State Override");

                std::string currentState = "Unknown";
                if (m_stateMachine.IsState("Idle")) currentState = "Idle [Thinking]";
                else if (m_stateMachine.IsState("Spawn Enemy")) currentState = "Spawn Enemy [Active]";
                else if (m_stateMachine.IsState("Intro")) currentState = "Intro [Cinematic]";
                else if (m_stateMachine.IsState("Lock Player")) currentState = "Lock Player [Attack]";
                else if (m_stateMachine.IsState("Download Attack")) currentState = "Download [Attack]";

                ImGui::TextColored({ 0,1,0,1 }, "Current State: %s", currentState.c_str());

                // Command Buttons
                if (ImGui::Button("Force IDLE"))          TriggerIdle();
                ImGui::SameLine();
                if (ImGui::Button("Force SPAWN ENEMY"))   TriggerSpawnEnemy();

                if (ImGui::Button("Force LOCK PLAYER"))   TriggerLockPlayer();
                ImGui::SameLine();
                if (ImGui::Button("Force DOWNLOAD"))
                {
                    ChangeState(new BossCommandState(new BossDownloadAttackState(), "DOWNLOADING FILES..."));
                }

                ImGui::Separator();

                // Debug Tree untuk Parts
                if (ImGui::TreeNode("Parts Transform Debug"))
                {
                    for (auto& part : m_parts)
                    {
                        if (ImGui::TreeNode(part->name.c_str()))
                        {
                            ImGui::DragFloat3("Pos", &part->position.x, 0.1f);
                            ImGui::DragFloat3("Rot", &part->rotation.x, 1.0f);
                            ImGui::DragFloat3("Scl", &part->scale.x, 0.1f);
                            ImGui::Checkbox("Float", &part->useFloating);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }

                // Background Settings
                ImGui::Separator();
                if (ImGui::TreeNode("Background Chain Settings"))
                {
                    ImGui::Checkbox("DEBUG: Force Render BG", &m_debugForceBG);
                    ImGui::DragFloat3("BG Position", &m_bgChainPos.x, 0.5f);
                    ImGui::DragFloat3("BG Rotation", &m_bgChainRotation.x, 1.0f);
                    ImGui::DragFloat2("BG Size", &m_bgChainSize.x, 0.5f);
                    if (ImGui::Button("Reset BG")) {
                        m_bgChainPos = { 0.0f, 0.0f, 30.0f };
                        m_bgChainSize = { 64.0f, 36.0f };
                    }
                    ImGui::TreePop();
                }

                ImGui::EndTabItem();
            }

            // =========================================================
            // TAB 2: ANTENNA TOOL (Alat khusus editing posisi attack)
            // =========================================================
            if (ImGui::BeginTabItem("Antenna Tool"))
            {
                ImGui::Spacing();
                ImGui::TextColored({ 0, 1, 1, 1 }, "ATTACK CONFIGURATION: DOWNLOAD");

                BossPart* p = HasPart("antenna") ? GetPart("antenna") : nullptr;

                if (!p) {
                    ImGui::TextColored({ 1, 0, 0, 1 }, "ERROR: Part 'antenna' not found!");
                }
                else {
                    // Opsi matikan floating biar gampang diedit
                    if (ImGui::Checkbox("Disable Floating for Edit", &m_showAntennaWindow)) {
                        p->useFloating = !m_showAntennaWindow; // Toggle logic
                    }
                    if (m_showAntennaWindow) p->useFloating = false; // Paksa mati kalau mode edit

                    ImGui::Separator();
                    ImGui::Text("1. Keyframe Positions (Start/End)");

                    // --- HIDDEN POS (START) ---
                    ImGui::PushID("Hidden");
                    if (ImGui::DragFloat3("Hidden Pos", &m_antennaHiddenPos.x, 0.1f)) {
                        p->position = m_antennaHiddenPos; // Live Preview
                    }
                    if (ImGui::Button("SNAP to Hidden")) {
                        p->position = m_antennaHiddenPos;
                    }
                    ImGui::PopID();

                    // --- SHOW POS (END) ---
                    ImGui::PushID("Show");
                    if (ImGui::DragFloat3("Show Pos", &m_antennaShowPos.x, 0.1f)) {
                        p->position = m_antennaShowPos; // Live Preview
                    }
                    if (ImGui::Button("SNAP to Show")) {
                        p->position = m_antennaShowPos;
                    }
                    ImGui::PopID();

                    ImGui::Separator();
                    ImGui::Text("2. Static Transform");

                    if (ImGui::DragFloat3("Rotation", &m_antennaRotation.x, 1.0f)) p->rotation = m_antennaRotation;
                    if (ImGui::DragFloat3("Scale", &m_antennaScale.x, 0.1f)) p->scale = m_antennaScale;

                    ImGui::Separator();
                    ImGui::Text("3. Projectile Settings");
                    ImGui::DragFloat3("Spawn Source", &m_fileSpawnSource.x, 0.1f);

                    if (ImGui::Button("Check Spawn Pos")) {
                        p->position = m_fileSpawnSource; // Pindahkan antena ke spawn point buat ngecek visual
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Current Antenna: (%.1f, %.1f, %.1f)", p->position.x, p->position.y, p->position.z);
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}