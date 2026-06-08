#include "System/Input.h"

void Input::Initialize(HWND hWnd)
{
	gamePad = std::make_unique<GamePad>();
	mouse = std::make_unique<Mouse>(hWnd);

	// --- TAMBAHKAN INI ---
	keyboard = std::make_unique<Keyboard>();
}

void Input::Update()
{
	gamePad->Update();
	mouse->Update();

	// --- TAMBAHKAN INI ---
	keyboard->Update();
}