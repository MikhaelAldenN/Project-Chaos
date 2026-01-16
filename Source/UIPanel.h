#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "Primitive.h"
#include "UIButtonPrimitive.h"
#include "BitmapFont.h"
#include "ResourceManager.h"

class UIPanel
{
public:
    UIPanel(Primitive* prim, float x, float y, float w, float h, const std::string& title);

    // Kontrol Tampilan
    void Show() { isVisible = true; }
    void Hide() { isVisible = false; }
    bool IsVisible() const { return isVisible; }

    // Setup Konten
    void SetMessage(const std::string& msg);
    void AddButton(const std::string& label, float relX, float relY, float w, float h, std::function<void()> onClick);

    // Core Loop (Penting untuk Modal Behavior)
    void Update(float dt); // Update tombol di dalam panel
    void Render(ID3D11DeviceContext* dc);

private:
    Primitive* primitive;

    // Layout
    float x, y, width, height;
    std::string title;
    std::vector<std::string> messageLines; // Pesan yang dipecah per baris

    // State
    bool isVisible = false;

    // Tombol-tombol milik panel ini (Yes/No, OK, dll)
    std::vector<std::unique_ptr<UIButtonPrimitive>> buttons;

    // Style Constants
    DirectX::XMFLOAT4 colorBg = { 0.0f, 0.0f, 0.0f, 1.0f };       // Hitam Pekat
    DirectX::XMFLOAT4 colorBorder = { 0.96f, 0.80f, 0.23f, 1.0f }; // Kuning DOS
    DirectX::XMFLOAT4 colorText = { 0.96f, 0.80f, 0.23f, 1.0f };   // Kuning DOS
};