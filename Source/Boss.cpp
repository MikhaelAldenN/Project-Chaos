#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>

using namespace DirectX;

void BossPart::Update(float dt)
{
    if (!model) return;

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

Boss::Boss()
{
    InitializeDefaultParts();

    // Init Terminal
    m_terminal.Initialize(Graphics::Instance().GetDevice(), 512, 512);
    m_terminal.AddLog("SYSTEM BOOT...");
    m_terminal.AddLog("WAITING FOR USER...");

    // Init Terminal untuk Monitor 1
    m_terminal1.Initialize(Graphics::Instance().GetDevice(), 512, 512);
    m_terminal1.AddLog("MONITOR 1 ONLINE");
    m_terminal1.AddLog("SYSTEM CHECK...");

    // === PERBAIKAN DI SINI ===
        // LOAD MODEL PLANE UNTUK LAYAR
    try {
        // 1. Load untuk Monitor Utama (Monitor 2)
        m_screenQuad = std::make_shared<Model>(Graphics::Instance().GetDevice(), "Data/Model/Primitive/Plane.glb");

        // 2. Load untuk Monitor 1 (Harus load instance baru agar texture bisa dibedakan)
        // Kita tidak bisa sekadar m_screenQuad1 = m_screenQuad karena texture terminalnya berbeda
        m_screenQuad1 = std::make_shared<Model>(Graphics::Instance().GetDevice(), "Data/Model/Primitive/Plane.glb");
    }
    catch (...) {
        OutputDebugStringA("WARNING: Plane.glb for Boss Terminal Screen not found!\n");
        // Handle error jika model tidak ketemu
        m_screenQuad = nullptr;
        m_screenQuad1 = nullptr;
    }

    m_stateMachine.Initialize(new BossIntroState(), this);
}

Boss::~Boss() {}

void Boss::InitializeDefaultParts()
{
    auto device = Graphics::Instance().GetDevice();

    std::vector<BossPartConfig> configs = {
        {
            "monitor1",
            "Data/Model/Character/TEST_mdl_CRTMonitor.glb",
            { 0.0f, 0.6f, 6.5f },
            { 65.0f, 0.0f, 0.0f },
            { 10.0f, 10.0f, 10.0f },
            true, 2.0f, 0.2f, { 0.0f, 1.0f, 0.0f }
        },
        {
            "cpu",
            "Data/Model/Character/TEST_mdl_CPU.glb",
            { 2.0f, -1.6f, 1.9f },
            { -86.0f, 0.0f, 176.0f },
            { 150.0f, 150.0f, 150.0f },
            true, 1.5f, 0.2f, { 1.0f, 1.0f, 0.0f }
        },
        {
            "monitor2",
            "Data/Model/Character/Test_mdl_CRTMonitor2.glb",
            { 5.5f, 0.0f, 4.2f },
            { -44.0f, 147.0f, 31.0f },
            { 100.0f, 100.0f, 100.0f },
            true, 1.0f, 0.05f, { 1.0f, 0.0f, -1.0f }
        },
        {
            "monitor3",
            "Data/Model/Character/TEST_mdl_CRTMonitor3.glb",
            { 5.0f, 2.3f, 7.5f },
            { 110.0f, 123.0f, 128.0f },
            { 100.0f, 100.0f, 100.0f },
            true, 1.0f, 0.25f, { 0.5f, 0.5f, 0.5f }
        },
        {
            "cable1",
            "Data/Model/Character/TEST_mdl_Cable1.glb",
            { -8.6f, 0.0f, 13.9f },
            { 90.0f, 0.0f, 0.0f },
            { 10.0f, 10.0f, 10.0f },
            true, 1.0f, 1.0f, { 1.0f, -1.0f, 0.0f }
        },
        {
            "cable2",
            "Data/Model/Character/TEST_mdl_Cable1.glb",
            { 6.3f, 5.5f, 12.9f },
            { 90.0f, 0.0f, 0.0f },
            { 8.0f, 8.0f, 8.0f },
            true, 0.5f, 0.5f, { -1.0f, 1.0f, 0.0f }
        },
        {
            "cable3",
            "Data/Model/Character/TEST_mdl_Cable3.glb",
            { 2.0f, 0.7f, 3.0f },
            { 156.0f, -16.0f, -133.0f },
            { 0.03f, 0.03f, 0.03f },
            true, 2.0f, 0.2f, { 0.0f, 1.0f, 0.0f }
        },
        {
            "platform1",
            "Data/Model/Character/TEST_mdl_Platform.glb",
            { -9.6f, 11.2f, 8.7f },
            { -62.0f, 138.0f, 48.0f },
            { 2.0f, 2.0f, 2.0f },
            true, 1.0f, 0.2f, { 0.0f, 1.0f, 0.0f }
        },
        {
            "platform2",
            "Data/Model/Character/TEST_mdl_Platform.glb",
            { 8.3f, 11.2f, 9.2f },
            { 43.0f, 60.0f, 59.0f },
            { 2.0f, 2.0f, 2.0f },
            true, 1.0f, 0.2f, { 0.0f, 1.0f, 0.0f }
        },
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

        char msg[256];
        sprintf_s(msg, "Loaded Boss Part: %s\n", config.name.c_str());
        OutputDebugStringA(msg);
    }
    catch (const std::exception& e) {
        char msg[512];
        sprintf_s(msg, "FAILED to load Boss Part '%s': %s\n",
            config.name.c_str(), e.what());
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

    m_partLookup[config.name] = newPart.get();
    m_parts.push_back(std::move(newPart));

    return true;
}

BossPart* Boss::GetPart(const std::string& name)
{
    auto it = m_partLookup.find(name);
    if (it != m_partLookup.end()) return it->second;
    return nullptr;
}

DirectX::XMFLOAT3 Boss::GetPartVisualPos(const std::string& name) const
{
    auto it = m_partLookup.find(name);
    if (it != m_partLookup.end()) return it->second->visualPosition;
    return { 0.0f, 0.0f, 0.0f };
}

bool Boss::HasPart(const std::string& name) const
{
    return m_partLookup.find(name) != m_partLookup.end();
}

std::vector<std::string> Boss::GetPartNames() const
{
    std::vector<std::string> names;
    names.reserve(m_parts.size());
    for (const auto& part : m_parts)
    {
        names.push_back(part->name);
    }
    return names;
}

void Boss::Update(float dt)
{
    for (auto& part : m_parts)
    {
        part->Update(dt);
    }

    m_terminal.Update(dt);
    m_terminal1.Update(dt);

    m_stateMachine.Update(this, dt);
}

void Boss::Render(ModelRenderer* renderer, Camera* camera)
{
    // 1. UPDATE TEXTURE TERMINAL (RTT)
    auto context = Graphics::Instance().GetDeviceContext();
    auto font = ResourceManager::Instance().GetFont("VGA_FONT");

    if (font) {
        m_terminal.RenderToTexture(context, font);
    }

    // Render Terminal 1 texture
    if (font) {
        m_terminal1.RenderToTexture(context, font);
    }

    // 2. RENDER BOSS PARTS BIASA
    for (auto& part : m_parts) {
        part->Render(renderer);
    }

    // 3. RENDER LAYAR MONITOR
    if (m_screenQuad && HasPart("monitor2"))
    {
        BossPart* monitor = GetPart("monitor2");

        // --- A. Hitung Transformasi ---
        XMMATRIX S = XMMatrixScaling(monitor->scale.x, monitor->scale.y, monitor->scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(monitor->rotation.x),
            XMConvertToRadians(monitor->rotation.y),
            XMConvertToRadians(monitor->rotation.z)
        );
        XMMATRIX T = XMMatrixTranslation(monitor->visualPosition.x, monitor->visualPosition.y, monitor->visualPosition.z);
        XMMATRIX matMonitor = S * R * T;

        XMMATRIX matScaleScreen = XMMatrixScaling(m_screenScale.x, m_screenScale.y, m_screenScale.z);
        XMMATRIX matRotScreen = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(m_screenRotation.x),
            XMConvertToRadians(m_screenRotation.y),
            XMConvertToRadians(m_screenRotation.z)
        );
        XMMATRIX matOffset = XMMatrixTranslation(m_screenOffset.x, m_screenOffset.y, m_screenOffset.z);

        // Gabungkan
        XMMATRIX matFinal = matScaleScreen * matRotScreen * matOffset * matMonitor;

        XMFLOAT4X4 worldFinal;
        XMStoreFloat4x4(&worldFinal, matFinal);

        m_screenQuad->UpdateTransform(worldFinal);

        // --- B. ASSIGN TEXTURE PERMANEN (FIX) ---
        // Kita tidak perlu "Restore" texture lama. 
        // Biarkan texture Terminal menempel selamanya di model ini.
        // Ini menjamin saat ModelRenderer bekerja nanti, texturenya masih benar.

        ID3D11ShaderResourceView* terminalTex = m_terminal.GetTexture();
        if (terminalTex)
        {
            const auto& meshes = m_screenQuad->GetMeshes();
            for (const auto& mesh : meshes)
            {
                if (mesh.material)
                {
                    // ComPtr assignment akan otomatis mengurus Reference Counting dengan aman
                    mesh.material->baseMap = terminalTex;
                }
            }
        }

        // --- C. DRAW ---
        // Gunakan Shader Basic (Unlit) agar terang dan tidak terpengaruh bayangan
        // Gunakan Shader Basic (Unlit) agar terang dan tidak terpengaruh bayangan
        renderer->Draw(ShaderId::Basic, m_screenQuad, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // 4. RENDER LAYAR MONITOR 1
    if (m_screenQuad1 && HasPart("monitor1"))
    {
        BossPart* monitor1 = GetPart("monitor1");

        // --- A. Hitung Transformasi Monitor 1 ---
        XMMATRIX S1 = XMMatrixScaling(monitor1->scale.x, monitor1->scale.y, monitor1->scale.z);
        XMMATRIX R1 = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(monitor1->rotation.x),
            XMConvertToRadians(monitor1->rotation.y),
            XMConvertToRadians(monitor1->rotation.z)
        );
        XMMATRIX T1 = XMMatrixTranslation(monitor1->visualPosition.x, monitor1->visualPosition.y, monitor1->visualPosition.z);
        XMMATRIX matMonitor1 = S1 * R1 * T1;

        XMMATRIX matScaleScreen1 = XMMatrixScaling(m_screen1Scale.x, m_screen1Scale.y, m_screen1Scale.z);
        XMMATRIX matRotScreen1 = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(m_screen1Rotation.x),
            XMConvertToRadians(m_screen1Rotation.y),
            XMConvertToRadians(m_screen1Rotation.z)
        );
        XMMATRIX matOffset1 = XMMatrixTranslation(m_screen1Offset.x, m_screen1Offset.y, m_screen1Offset.z);

        XMMATRIX matFinal1 = matScaleScreen1 * matRotScreen1 * matOffset1 * matMonitor1;

        XMFLOAT4X4 worldFinal1;
        XMStoreFloat4x4(&worldFinal1, matFinal1);
        m_screenQuad1->UpdateTransform(worldFinal1);

        // --- B. ASSIGN TEXTURE MONITOR 1 ---
        ID3D11ShaderResourceView* terminal1Tex = m_terminal1.GetTexture();
        if (terminal1Tex)
        {
            const auto& meshes = m_screenQuad1->GetMeshes();
            for (const auto& mesh : meshes)
            {
                if (mesh.material)
                {
                    mesh.material->baseMap = terminal1Tex;
                }
            }
        }

        // --- C. DRAW MONITOR 1 ---
        renderer->Draw(ShaderId::Basic, m_screenQuad1, { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    else
    {
        // Debug: Print why monitor1 screen not rendering
        static bool loggedOnce = false;
        if (!loggedOnce)
        {
            if (!m_screenQuad1)
                OutputDebugStringA("DEBUG: Monitor1 screen not rendering - m_screenQuad1 is NULL\n");
            if (!HasPart("monitor1"))
                OutputDebugStringA("DEBUG: Monitor1 screen not rendering - monitor1 part not found\n");
            loggedOnce = true;
        }
    }
}

void Boss::AddTerminalLog(const std::string& msg)
{
    // m_terminal.AddLog(msg);
}

// [Helper] Biar gampang ganti state
void Boss::ChangeState(BossState* newState)
{
    m_stateMachine.ChangeState(this, newState);
}

// --- IMPLEMENTASI AI COMMANDS ---

void Boss::TriggerIdle()
{
    // Idle biasanya langsung, tidak perlu ngetik di terminal
    ChangeState(new BossIdleState());
}

void Boss::TriggerSpawnEnemy()
{
    // Bungkus logika "Ngetik dulu baru Spawn" di sini
    ChangeState(new BossCommandState(
        new BossSpawnEnemyState(),
        "DEPLOY MINIONS"
    ));
}

void Boss::TriggerLockPlayer()
{
    // Bungkus logika "Ngetik dulu baru Lock" di sini
    ChangeState(new BossCommandState(
        new BossLockPlayerState(),
        "INITIATE LOCK"
    ));
}

void Boss::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Status Indicators
        ImGui::TextColored({ 1,1,0,1 }, "=== TERMINAL STATUS ===");

        // Monitor 2 Status
        bool mon2Ready = m_screenQuad && HasPart("monitor2");
        ImGui::TextColored(mon2Ready ? ImVec4{ 0,1,0,1 } : ImVec4{ 1,0,0,1 },
            "Monitor 2: %s", mon2Ready ? "ACTIVE" : "INACTIVE");

        // Monitor 1 Status
        bool mon1Ready = m_screenQuad1 && HasPart("monitor1");
        ImGui::TextColored(mon1Ready ? ImVec4{ 0,1,0,1 } : ImVec4{ 1,0,0,1 },
            "Monitor 1: %s", mon1Ready ? "ACTIVE" : "INACTIVE");

        if (!m_screenQuad1) ImGui::TextColored({ 1,0.5,0,1 }, "  > Screen Quad 1 not loaded");
        if (!HasPart("monitor1")) ImGui::TextColored({ 1,0.5,0,1 }, "  > Monitor1 part not found");

        ImGui::Separator();

        m_terminal.DrawGUI();

        ImGui::Separator();
        m_terminal1.DrawGUI();

        // --- [BARU] STATE CONTROLS ---
        ImGui::Text("AI State Override");

        // Tampilkan State yang sedang aktif
        std::string currentState = "Unknown";
        if (m_stateMachine.IsState("Idle")) currentState = "Idle";
        else if (m_stateMachine.IsState("Spawn Enemy")) currentState = "Spawn Enemy";
        else if (m_stateMachine.IsState("Intro")) currentState = "Intro";

        ImGui::TextColored({ 0,1,0,1 }, "Current State: %s", currentState.c_str());

        // Tombol Trigger
        if (ImGui::Button("Force IDLE"))
        {
            TriggerIdle(); // Bersih!
        }

        ImGui::SameLine();

        if (ImGui::Button("Force SPAWN ENEMY"))
        {
            TriggerSpawnEnemy(); // Bersih!
        }

        ImGui::SameLine();

        if (ImGui::Button("Force LOCK PLAYER"))
        {
            TriggerLockPlayer(); // Bersih!
        }
        ImGui::Separator();


        ImGui::Text("Active Parts: %d", (int)m_parts.size());
        ImGui::Separator();

        for (auto& part : m_parts)
        {
            ImGui::PushID(part->name.c_str());

            if (ImGui::TreeNode(part->name.c_str()))
            {
                ImGui::DragFloat3("Position", &part->position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &part->rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &part->scale.x, 0.1f);

                ImGui::Checkbox("Floating", &part->useFloating);

                if (part->useFloating)
                {
                    ImGui::DragFloat("Speed", &part->floatSpeed, 0.01f);
                    ImGui::DragFloat("Intensity", &part->floatIntensity, 0.01f);
                    ImGui::DragFloat3("Axis", &part->floatAxis.x, 0.1f, -1.0f, 1.0f);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (ImGui::TreeNode("Terminal Screen Transform"))
        {
            ImGui::Text("Adjust Screen Placement:");

            // Gunakan DragFloat dengan step kecil (0.01f) untuk presisi
            ImGui::DragFloat3("Offset (Local)", &m_screenOffset.x, 0.005f);
            ImGui::DragFloat3("Rotation (Local)", &m_screenRotation.x, 1.0f);
            ImGui::DragFloat3("Scale (Local)", &m_screenScale.x, 0.005f);

            if (ImGui::Button("Reset to Default"))
            {
                m_screenOffset = { 0.0f, 0.25f, 0.55f };
                m_screenScale = { 0.75f, 0.55f, 0.01f };
                m_screenRotation = { 0.0f, 0.0f, 0.0f };
            }

            ImGui::TreePop();

            if (ImGui::TreeNode("Monitor 1 Screen Transform"))
            {
                ImGui::Text("Adjust Monitor 1 Screen Placement:");
                ImGui::DragFloat3("Offset##mon1", &m_screen1Offset.x, 0.005f);
                ImGui::DragFloat3("Rotation##mon1", &m_screen1Rotation.x, 1.0f);
                ImGui::DragFloat3("Scale##mon1", &m_screen1Scale.x, 0.005f);

                if (ImGui::Button("Reset Monitor 1##mon1"))
                {
                    m_screen1Offset = { 0.0f, 0.02f, 0.055f };
                    m_screen1Scale = { 0.75f, 0.55f, 0.01f };
                    m_screen1Rotation = { 0.0f, 0.0f, 0.0f };
                }

                ImGui::TreePop();
            }
        }
        ImGui::Separator();

    }
}