#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h" // Untuk ambil Font
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <vector>

using namespace DirectX;

// =========================================================
// BOSS PART IMPLEMENTATION
// =========================================================
void BossPart::Update(float dt)
{
    if (!model) return;

    // 1. Logika Floating
    float offsetX = 0.0f, offsetY = 0.0f, offsetZ = 0.0f;

    if (useFloating)
    {
        timer += dt;
        float wave = sin(timer * floatSpeed) * floatIntensity;
        offsetX = wave * floatAxis.x;
        offsetY = wave * floatAxis.y;
        offsetZ = wave * floatAxis.z;
    }

    // 2. Simpan Visual Position (Base Pos + Animasi)
    visualPosition = { position.x + offsetX, position.y + offsetY, position.z + offsetZ };

    // 3. Update Matrix ke GPU
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

    m_terminal.Initialize(10);

    // Opsional: Tambahkan pesan awal
    m_terminal.AddLog("SYSTEM BOOT SEQUENCE...");
    m_terminal.AddLog("LOADING KERNEL...");
}

Boss::~Boss() {}

void Boss::InitializeDefaultParts()
{
    auto device = Graphics::Instance().GetDevice();

    // =====================================================
    // KONFIGURASI PARTS - EDIT DI SINI UNTUK TAMBAH/UBAH!
    // =====================================================

    std::vector<BossPartConfig> configs = {
        // Monitor 1 (Main Head)
        {
            "monitor1",
            "Data/Model/Character/TEST_mdl_CRTMonitor.glb",
            { 0.0f, 0.6f, 6.5f },        // position
            { 65.0f, 0.0f, 0.0f },       // rotation
            { 10.0f, 10.0f, 10.0f },     // scale
            true,                         // useFloating
            2.0f,                         // floatSpeed
            0.2f,                         // floatIntensity
            { 0.0f, 1.0f, 0.0f }         // floatAxis (Y up/down)
        },

        // CPU (Body)
        {
            "cpu",
            "Data/Model/Character/TEST_mdl_CPU.glb",
            { 2.0f, -1.6f, 1.9f },
            { -86.0f, 0.0f, 176.0f },
            { 150.0f, 150.0f, 150.0f },
            true,
            1.5f,
            0.2f,
            { 1.0f, 1.0f, 0.0f }        // XY diagonal float
        },

        // Monitor 2 (Side Left)
        {
            "monitor2",
            "Data/Model/Character/Test_mdl_CRTMonitor2.glb",
            { 5.5f, 0.0f, 4.2f },
            { -44.0f, 147.0f, 31.0f },
            { 100.0f, 100.0f, 100.0f },
            true,
            1.0f,
            0.05f,
            { 1.0f, 0.0f, -1.0f }
        },

        // Monitor 3 (Side Right) - BARU!
        {
            "monitor3",
            "Data/Model/Character/TEST_mdl_CRTMonitor3.glb",
            { 5.0f, 2.3f, 7.5f },        // Mirror position dari monitor2
            { 110.0f, 123.0f, 128.0f },      // Rotation menghadap ke tengah
            { 100.0f, 100.0f, 100.0f },
            true,
            1.0f,                         // Sedikit beda speed
            0.25f,                        // Sedikit lebih intens
            { 0.5f, 0.5f, 0.5f }
        },

        {
            "cable1",
            "Data/Model/Character/TEST_mdl_Cable1.glb",
            { -8.6f, 0.0f, 13.9f },
            { 90.0f, 0.0f, 0.0f },
            { 10.0f, 10.0f, 10.0f },
            true,
            1.0f,
            1.0f,
            { 1.0f, -1.0f, 0.0f }
        },

        {
            "cable2",
            "Data/Model/Character/TEST_mdl_Cable1.glb",
            { 6.3f, 5.5f, 12.9f },
            { 90.0f, 0.0f, 0.0f },
            { 8.0f, 8.0f, 8.0f },
            true,
            0.5f,
            0.5f,
            { -1.0f, 1.0f, 0.0f }
        },

        {
            "cable3",
            "Data/Model/Character/TEST_mdl_Cable3.glb",
            { 2.0f, 0.7f, 3.0f },
            { 156.0f, -16.0f, -133.0f },
            { 0.03f, 0.03f, 0.03f },
            true,
            2.0f,
            0.2f,
            { 0.0f, 1.0f, 0.0f }
        },

        {
            "platform1",
            "Data/Model/Character/TEST_mdl_Platform.glb",
            { -9.6f, 11.2f, 8.7f },
            { -62.0f, 138.0f, 48.0f },
            { 2.0f, 2.0f, 2.0f },
            true,
            1.0f,
            0.2f,
            { 0.0f, 1.0f, 0.0f }
        },
    
        {
            "platform2",
            "Data/Model/Character/TEST_mdl_Platform.glb",
            { 8.3f, 11.2f, 9.2f },
            { 43.0f, 60.0f, 59.0f },
            { 2.0f, 2.0f, 2.0f },
            true,
            1.0f,
            0.2f,
            { 0.0f, 1.0f, 0.0f }
        },

        //{
        //    "laser1",
        //    "Data/Model/Character/TEST_mdl_LaserGun.glb",
        //    { 16.0f, 0.0f, 3.5f },
        //    { 0.0f, 0.0f, -90.0f },
        //    { 150.0f, 150.0f, 150.0f },
        //    false,
        //    1.0f,
        //    0.2f,
        //    { 0.0f, 1.0f, 0.0f }
        //}



        // CARA TAMBAH PART BARU:
        // Tinggal copy paste block di atas, ubah name, modelPath, dan transform!
        /*
        {
            "keyboard",
            "Data/Model/Character/TEST_mdl_Keyboard.glb",
            { 0.0f, -2.0f, 3.0f },
            { 0.0f, 0.0f, 0.0f },
            { 5.0f, 5.0f, 5.0f },
            true,
            3.0f,
            0.1f,
            { 1.0f, 0.0f, 0.0f }
        }
        */
    };

    // Load semua parts
    for (const auto& config : configs)
    {
        AddPart(config);
    }
}

bool Boss::AddPart(const BossPartConfig& config)
{
    auto device = Graphics::Instance().GetDevice();

    // Create new part
    auto newPart = std::make_unique<BossPart>();
    newPart->name = config.name;

    // Try load model
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

        // Return false tapi JANGAN crash game
        return false;
    }

    // Setup transform
    newPart->position = config.position;
    newPart->rotation = config.rotation;
    newPart->scale = config.scale;

    // Setup floating
    newPart->useFloating = config.useFloating;
    newPart->floatSpeed = config.floatSpeed;
    newPart->floatIntensity = config.floatIntensity;
    newPart->floatAxis = config.floatAxis;

    // Add to lookup map
    m_partLookup[config.name] = newPart.get();

    // Add to vector
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
}

void Boss::Render(ModelRenderer* renderer, Camera* camera)
{
    // 1. Render Models
    for (auto& part : m_parts) part->Render(renderer);

    // 2. Render Text
    if (camera)
    {
        if (m_partLookup.find("monitor1") != m_partLookup.end())
        {
            BossPart* monitor = m_partLookup["monitor1"];
            BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");

            DirectX::XMFLOAT3 radRot = {
                DirectX::XMConvertToRadians(monitor->rotation.x),
                DirectX::XMConvertToRadians(monitor->rotation.y),
                DirectX::XMConvertToRadians(monitor->rotation.z)
            };

            // Delegate rendering ke object terminal
            m_terminal.Render(font, camera, monitor->visualPosition, radRot);
        }
    }
}

void Boss::AddTerminalLog(const std::string& msg)
{
    m_terminal.AddLog(msg);
}

void Boss::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen))
    {

        if (ImGui::TreeNode("Terminal Layout (Edit Here!)"))
        {
            ImGui::TextColored({0,1,0,1}, "Gunakan slider ini untuk pasin posisi!");
            
            // Kontrol Posisi (Offset dari tengah monitor)
            ImGui::DragFloat3("Pos Offset", &m_terminal.offset.x, 0.1f);
            
            // Kontrol Rotasi Tambahan
            ImGui::DragFloat3("Rot Offset", &m_terminal.rotationOffset.x, 1.0f, -180.0f, 180.0f);
            
            // Kontrol Skala & Spasi
            ImGui::DragFloat("Scale", &m_terminal.scale, 0.001f, 0.001f, 0.1f);
            ImGui::DragFloat("Line Spacing", &m_terminal.lineSpacing, 0.05f);
            
            // Tombol Reset untuk mempermudah kalau berantakan
            if (ImGui::Button("Reset to Default")) {
                m_terminal.offset = { -2.3f, 3.8f, 0.0f };
                m_terminal.rotationOffset = { 0.0f, 0.0f, 0.0f };
                m_terminal.scale = 0.011f;
            }

            ImGui::TreePop();
            ImGui::Separator();
        }

        ImGui::Text("Active Parts: %d", (int)m_parts.size());
        ImGui::Separator();

        for (auto& part : m_parts)
        {
            ImGui::PushID(part->name.c_str());

            if (ImGui::TreeNode(part->name.c_str()))
            {
                ImGui::Text("Transform:");
                ImGui::DragFloat3("Position", &part->position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &part->rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &part->scale.x, 0.1f);

                ImGui::Separator();
                ImGui::Checkbox("Floating Anim", &part->useFloating);

                if (part->useFloating)
                {
                    ImGui::DragFloat("Speed", &part->floatSpeed, 0.01f);
                    ImGui::DragFloat("Intensity", &part->floatIntensity, 0.01f);
                    ImGui::DragFloat3("Axis", &part->floatAxis.x, 0.1f, -1.0f, 1.0f);
                }

                ImGui::Text("Visual Pos: (%.2f, %.2f, %.2f)",
                    part->visualPosition.x,
                    part->visualPosition.y,
                    part->visualPosition.z);

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }
}