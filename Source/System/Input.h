#pragma once

#include <memory>
#include <cstdint> 
#include "System/GamePad.h"
#include "System/Mouse.h"
#include "System/Keyboard.h"

// AAA Standard: Global Device Tracker Enum
enum class InputDevice : std::uint8_t {
    Keyboard = 0,
    Gamepad
};

class Input
{
private:
    Input() = default;
    ~Input() = default;

public:
    static Input& Instance()
    {
        static Input instance;
        return instance;
    }

    void Initialize(HWND hWnd);
    void Update();

    GamePad& GetGamePad() { return *gamePad; }
    Mouse& GetMouse() { return *mouse; }
    Keyboard& GetKeyboard() { return *keyboard; }

    // --- GLOBAL GETTERS / SETTERS ---
    [[nodiscard]] InputDevice GetLastUsedDevice() const noexcept { return m_lastUsedDevice; }
    void SetLastUsedDevice(InputDevice device) noexcept { m_lastUsedDevice = device; }

private:
    std::unique_ptr<GamePad>	gamePad;
    std::unique_ptr<Mouse>		mouse;
    std::unique_ptr<Keyboard>	keyboard;

    // Will hold its state across all scene changes
    InputDevice m_lastUsedDevice{ InputDevice::Keyboard };
};