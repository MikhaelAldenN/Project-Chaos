#include "Button.h"
#include "System/Input.h" 
#include "System/Graphics.h" // Untuk akses Graphics device saat load sprite

Button::Button(const std::string& texturePath, float x, float y, float w, float h)
    : Button(texturePath, x, y, w, h,
        { 1.0f, 1.0f, 1.0f },   // Standby (Normal)
        { 1.2f, 1.2f, 1.2f },   // Hover (Lebih terang)
        { 0.8f, 0.8f, 0.8f })   // Pressed (Lebih gelap)
{
}

Button::Button(const std::string& texturePath, float x, float y, float w, float h,
    ButtonTints standby, ButtonTints hover, ButtonTints pressed)
    : width(w), height(h),
    m_standbyTint(standby), m_hoverTint(hover), m_pressedTint(pressed)
{
    // Load Sprite menggunakan Device dari Singleton Graphics
    sprite = new Sprite(Graphics::Instance().GetDevice(), texturePath.c_str());

    currentState = ButtonState::STANDBY;

    // Setup RECT
    SetPosition(x, y);
}

Button::~Button()
{
    if (sprite)
    {
        delete sprite;
        sprite = nullptr;
    }
}

void Button::SetPosition(float x, float y)
{
    rect.left = static_cast<long>(x);
    rect.top = static_cast<long>(y);
    rect.right = static_cast<long>(x + width);
    rect.bottom = static_cast<long>(y + height);
}

void Button::SetOnClick(OnClickCallback callback)
{
    m_onClick = callback;
}

void Button::Update()
{
    Mouse& mouse = Input::Instance().GetMouse();
    POINT mousePos = { mouse.GetPositionX(), mouse.GetPositionY() };

    bool isHover = PtInRect(&rect, mousePos);
    bool isMouseDown = mouse.GetButtonDown() & Mouse::BTN_LEFT;

    if (isHover)
    {
        if (isMouseDown)
        {
            currentState = ButtonState::PRESSED;
        }
        else
        {
            // Jika sebelumnya PRESSED dan sekarang lepas mouse di dalam tombol -> CLICK!
            if (currentState == ButtonState::PRESSED)
            {
                if (m_onClick) m_onClick(); // FIRE EVENT!
            }
            currentState = ButtonState::HOVER;
        }
    }
    else
    {
        currentState = ButtonState::STANDBY;
    }
}

void Button::Render(ID3D11DeviceContext* dc, Camera* camera)
{
    if (!sprite) return;

    // Tentukan warna berdasarkan state
    float r = 1.0f, g = 1.0f, b = 1.0f;
    switch (currentState)
    {
    case ButtonState::STANDBY: r = m_standbyTint.r; g = m_standbyTint.g; b = m_standbyTint.b; break;
    case ButtonState::HOVER:   r = m_hoverTint.r;   g = m_hoverTint.g;   b = m_hoverTint.b;   break;
    case ButtonState::PRESSED: r = m_pressedTint.r; g = m_pressedTint.g; b = m_pressedTint.b; break;
    }

    // Render Sprite
    // Kita asumsikan Sprite::Render parameternya sama dengan yang dipakai di SceneTitle
    sprite->Render(
        dc,
        camera,
        static_cast<float>(rect.left), static_cast<float>(rect.top), 0.0f, // X, Y, Z
        width, height, // Width, Height
        0.0f, 0.0f, 0.0f, // Rotation
        r, g, b, m_alpha // Color & Alpha
    );
}

void Button::SetAlpha(float alpha)
{
    if (alpha < 0.0f) m_alpha = 0.0f;
    else if (alpha > 1.0f) m_alpha = 1.0f;
    else m_alpha = alpha;
}