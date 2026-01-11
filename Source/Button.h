#pragma once

#include <functional>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h> // Pastikan include ini ada untuk ID3D11DeviceContext

#include "System/Sprite.h"

// Callback type definition
using OnClickCallback = std::function<void()>;

enum class ButtonState
{
    STANDBY,
    HOVER,
    PRESSED
};

struct ButtonTints
{
    float r, g, b;
};

class Button
{
public:
    // Constructor
    Button(const std::string& texturePath, float x, float y, float width, float height);
    Button(const std::string& texturePath, float x, float y, float width, float height,
        ButtonTints standby, ButtonTints hover, ButtonTints pressed);

    ~Button(); // Destructor tetap perlu untuk membersihkan Sprite* (kecuali Sprite juga pakai unique_ptr)

    // Core Loop
    void Update();

    // Render disesuaikan dengan SceneTitle: butuh DeviceContext & Camera
    // Camera opsional (nullptr) jika ingin render murni 2D tanpa view matrix
    void Render(ID3D11DeviceContext* dc, class Camera* camera = nullptr);

    // Modern Event System
    void SetOnClick(OnClickCallback callback);

    // Property Setters
    void SetAlpha(float alpha);
    void SetPosition(float x, float y);

    // Getters
    bool IsPressed() const { return currentState == ButtonState::PRESSED; }
    ButtonState GetState() const { return currentState; }

private:
    Sprite* sprite = nullptr; // Raw pointer karena Sprite.h mungkin belum pakai unique_ptr

    // Layout
    RECT rect = {};
    float width, height;

    // State
    ButtonState currentState = ButtonState::STANDBY;
    bool isPressedOnFrame = false;

    // Visuals
    ButtonTints m_standbyTint;
    ButtonTints m_hoverTint;
    ButtonTints m_pressedTint;
    float m_alpha = 1.0f;

    // Event
    OnClickCallback m_onClick;
};