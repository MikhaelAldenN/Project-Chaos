#pragma once

#include <memory>
#include "System/GamePad.h"
#include "System/Mouse.h"
#include "System/Keyboard.h" // <--- TAMBAHKAN INI

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

	// --- TAMBAHKAN GETTER INI ---
	Keyboard& GetKeyboard() { return *keyboard; }

private:
	std::unique_ptr<GamePad>	gamePad;
	std::unique_ptr<Mouse>		mouse;

	// --- TAMBAHKAN VARIABLE INI ---
	std::unique_ptr<Keyboard>	keyboard;
};