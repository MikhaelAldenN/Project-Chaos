#include "ButtonManager.h"

void ButtonManager::AddButton(std::unique_ptr<Button> button)
{
    // std::move memindahkan kepemilikan pointer ke dalam vector
    buttons.push_back(std::move(button));
}

void ButtonManager::Update()
{
    for (auto& btn : buttons)
    {
        btn->Update();
    }
}

void ButtonManager::Render(ID3D11DeviceContext* dc, Camera* camera)
{
    for (auto& btn : buttons)
    {
        btn->Render(dc, camera);
    }
}

void ButtonManager::Clear()
{
    buttons.clear();
}