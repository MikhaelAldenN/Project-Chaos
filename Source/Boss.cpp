#include "Boss.h"
#include "System/Graphics.h"

using namespace DirectX;

Boss::Boss()
{
    auto device = Graphics::Instance().GetDevice();

    // Load Model sesuai path yang kamu minta
    // Pastikan file "TEST_CRTMontitor.glb" ada di folder "Data/Model/Character/"
    model = std::make_shared<Model>(device, "Data/Model/Character/TEST_mdl_CRTMonitor.glb");
}

Boss::~Boss()
{
}

void Boss::Update(float dt)
{
    if (!model) return;

    // [Simple Animation] Sedikit melayang (Floating)
    static float timer = 0.0f;
    timer += dt;
    float floatOffset = sin(timer * 2.0f) * 0.5f; // Naik turun halus

    // Update Matrix Transform
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );
    // Tambahkan floatOffset ke posisi Y
    XMMATRIX T = XMMatrixTranslation(position.x, position.y + floatOffset, position.z);

    XMMATRIX world = S * R * T;

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);

    model->UpdateTransform(worldFloat);
}

void Boss::Render(ModelRenderer* renderer)
{
    if (!model) return;

    // Render dengan warna putih normal (1,1,1,1)
renderer->Draw(ShaderId::Lambert, model, { 1.0f, 1.0f, 1.0f, 1.0f });}