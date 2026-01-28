#include "Boss.h"
#include "System/Graphics.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <imgui.h>
#include <cmath>
#include <vector>

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

    // ===================================================
    //  TERMINAL DISABLED FOR PERFORMANCE TESTING
    // ===================================================
    // m_terminal.Initialize(10);
    // m_terminal.maxVisibleLines = 6;
    // m_terminal.updateInterval = 0.1f;
    // m_terminal.enableTypewriter = true;
    // m_terminal.AddLog("SYSTEM BOOT SEQUENCE...");
    // m_terminal.AddLog("LOADING KERNEL...");
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

    // ===================================================
    //  TERMINAL UPDATE DISABLED
    // ===================================================
    // m_terminal.Update(dt);
}

void Boss::Render(ModelRenderer* renderer, Camera* camera)
{
    for (auto& part : m_parts) part->Render(renderer);

    // ===================================================
    //  TERMINAL RENDER DISABLED - THIS IS THE FPS KILLER
    // ===================================================
    /*
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

            m_terminal.Render(font, camera, monitor->visualPosition, radRot);
        }
    }
    */
}

void Boss::AddTerminalLog(const std::string& msg)
{
    // m_terminal.AddLog(msg);
}

void Boss::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored({ 1,1,0,1 }, "TERMINAL DISABLED FOR PERFORMANCE");
        ImGui::Text("Check Boss_NO_TERMINAL.cpp to re-enable");
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
    }
}