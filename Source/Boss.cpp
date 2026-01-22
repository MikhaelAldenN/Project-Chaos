#include "Boss.h"
#include "System/Graphics.h"
#include <imgui.h>
#include <cmath>

using namespace DirectX;

Boss::Boss()
{
    auto device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, "Data/Model/Character/TEST_mdl_CRTMonitor.glb");
}

Boss::~Boss()
{
}

void Boss::Update(float dt)
{
    if (!model) return;

    // 1. Logika Floating
    float offsetX = 0.0f, offsetY = 0.0f, offsetZ = 0.0f;

    if (useFloatingAnim)
    {
        timer += dt;
        float wave = sin(timer * floatSpeed) * floatIntensity;
        offsetX = wave * floatAxis.x;
        offsetY = wave * floatAxis.y;
        offsetZ = wave * floatAxis.z;
    }

    // 2. Hitung Matrix
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    // Simpan posisi visual final untuk referensi luar
    visualPosition = { position.x + offsetX, position.y + offsetY, position.z + offsetZ };

    XMMATRIX T = XMMatrixTranslation(visualPosition.x, visualPosition.y, visualPosition.z);
    XMMATRIX world = S * R * T;

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);
    model->UpdateTransform(worldFloat);
}

void Boss::Render(ModelRenderer* renderer)
{
    if (!model) return;
    renderer->Draw(ShaderId::Lambert, model, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Boss::DrawDebugGUI()
{
    if (ImGui::CollapsingHeader("Boss Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Transform (Base):");
        ImGui::DragFloat3("Position", &position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &rotation.x, 1.0f);
        ImGui::DragFloat3("Scale", &scale.x, 0.1f);

        ImGui::Separator();

        ImGui::Text("Floating Animation:");
        ImGui::Checkbox("Enable", &useFloatingAnim);

        if (useFloatingAnim)
        {
            ImGui::DragFloat("Speed", &floatSpeed, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Intensity", &floatIntensity, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat3("Direction Axis", &floatAxis.x, 0.1f, -1.0f, 1.0f);
        }

        if (ImGui::Button("Reset All"))
        {
            // Nilai Default sesuai Screenshot User
            position = { 0.0f, 0.6f, 6.5f };
            rotation = { 65.0f, 0.0f, 0.0f };
            scale = { 10.0f, 10.0f, 10.0f };

            useFloatingAnim = true;
            floatSpeed = 0.5f;
            floatIntensity = 0.2f;
            floatAxis = { 0.0f, 0.0f, 1.0f };
        }
    }
}