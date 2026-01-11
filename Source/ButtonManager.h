#pragma once

#include <vector>
#include <memory> // Penting untuk unique_ptr
#include "Button.h"

class ButtonManager
{
public:
    ButtonManager() = default;
    ~ButtonManager() = default; // Default destructor sudah cukup karena unique_ptr membersihkan dirinya sendiri

    // Terima unique_ptr (move ownership)
    void AddButton(std::unique_ptr<Button> button);

    void Update();

    // Render butuh context & camera
    void Render(ID3D11DeviceContext* dc, Camera* camera = nullptr);

    // Hapus semua tombol (misal saat ganti scene)
    void Clear();

private:
    std::vector<std::unique_ptr<Button>> buttons;
};