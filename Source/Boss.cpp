#include "Boss.h"
#include "System/Graphics.h"
#include <imgui.h>
#include <cmath>

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

// Di Boss.cpp Constructor
Boss::Boss()
{
    auto device = Graphics::Instance().GetDevice();

    // 1. Init Monitor (CRT)
    try {
        partMonitor.model = std::make_shared<Model>(device,
            "Data/Model/Character/TEST_mdl_CRTMonitor.glb");
    }
    catch (const std::exception& e) {
        OutputDebugStringA("Failed to load Monitor model!\n");
        OutputDebugStringA(e.what());
    }

    partMonitor.position = { 0.0f, 0.6f, 6.5f };
    partMonitor.rotation = { 65.0f, 0.0f, 0.0f };
    partMonitor.scale = { 10.0f, 10.0f, 10.0f };
    partMonitor.floatAxis = { 0.0f, 1.0f, 0.0f };

    // 2. Init CPU
    try {
        partCPU.model = std::make_shared<Model>(device,
            "Data/Model/Character/TEST_mdl_CPU.glb");
    }
    catch (const std::exception& e) {
        OutputDebugStringA("Failed to load CPU model!\n");
        OutputDebugStringA(e.what());
    }

    partCPU.position = { 2.6f, -1.6f, 1.0f };
    partCPU.rotation = { -86.0f, 0.0f, 176.0f };
    partCPU.scale = { 150.0f, 150.0f, 150.0f };
    partCPU.floatSpeed = 1.5f;
    partCPU.floatAxis = { 1.0f, 1.0f, 0.0f };

    // 3. Init Monitor 2 (CRITICAL!)
    try {
        partMonitor2.model = std::make_shared<Model>(device,
            "Data/Model/Character/Test_mdl_CRTMonitor2.glb");

        OutputDebugStringA("Monitor2 model loaded successfully!\n");
    }
    catch (const std::exception& e) {
        OutputDebugStringA(" FAILED to load Monitor2 model!\n");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");

        // [FALLBACK] Use Monitor 1 model sebagai backup
        partMonitor2.model = partMonitor.model;
    }

    partMonitor2.position = { 5.5f, 0.0f, 4.2f };
    partMonitor2.rotation = { -44.0f, 147.0f, 31.0f };
    partMonitor2.scale = { 100.0f, 100.0f, 100.0f };
    partMonitor2.floatSpeed = 1.8f;
    partMonitor2.floatIntensity = 0.2f;
    partMonitor2.floatAxis = { 0.0f, 1.0f, 0.0f };
}

Boss::~Boss() {}

void Boss::Update(float dt)
{
    partMonitor.Update(dt);
    partCPU.Update(dt);
    partMonitor2.Update(dt);
}

void Boss::Render(ModelRenderer* renderer)
{
    partMonitor.Render(renderer);
    partCPU.Render(renderer);
    partMonitor2.Render(renderer);
}

void Boss::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Boss Manager", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto DrawPartGUI = [](const char* name, BossPart& part) {
            ImGui::PushID(name);
            if (ImGui::TreeNode(name))
            {
                ImGui::Text("Transform:");
                ImGui::DragFloat3("Position", &part.position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &part.rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &part.scale.x, 0.1f);

                ImGui::Separator();
                ImGui::Checkbox("Floating Anim", &part.useFloating);
                if (part.useFloating) {
                    ImGui::DragFloat("Speed", &part.floatSpeed, 0.01f);
                    ImGui::DragFloat("Intensity", &part.floatIntensity, 0.01f);
                    ImGui::DragFloat3("Axis", &part.floatAxis.x, 0.1f, -1.0f, 1.0f);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            };

        DrawPartGUI("CRT Monitor (Head)", partMonitor);
        DrawPartGUI("CPU Tower (Body)", partCPU);
        DrawPartGUI("CRT Monitor 2 (Side)", partMonitor2);
    }
}