#pragma once
#include <vector>
#include <memory>
#include "Item.h"
#include "System/Graphics.h"
#include "System/ShapeRenderer.h"

struct ItemSpawnData {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation;
    DirectX::XMFLOAT3 Scale;
    ItemType Type;
};

// ==========================================
// ITEM CONFIGURATION (EDIT HERE)
// ==========================================
namespace ItemLevelData
{
    static const std::vector<ItemSpawnData> Spawns =
    {
        // ------------------------------------------
        // HEAL LIST
        // ------------------------------------------
        // Item Heal #1
        { { -2.8f, 0.4f, -164.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #2
        { { -4.2f, 0.4f, -164.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #3
        { { -5.7f, 0.4f, -164.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #4
        { { -2.8f, 0.4f, -166.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #5
        { { -4.2f, 0.4f, -166.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #6
        { { -5.7f, 0.4f, -166.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #7
        { { -2.8f, 0.4f, -167.8f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #8
        { { -4.2f, 0.4f, -167.8f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #9
        { { -5.7f, 0.4f, -167.8f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #10
        { { 11.6f, 0.4f, -215.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #11
        { { 9.9f, 0.4f, -215.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #12
        { { 8.2f, 0.4f, -215.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #13
        { { 12.1f, 0.4f, -216.9f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #14
        { { 10.5f, 0.4f, -216.9f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #15
        { { 8.9f, 0.4f, -216.9f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #16
        { { 9.1f, 0.4f, -261.2f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #17
        { { 8.7f, 0.4f, -259.7f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #18
        { { 8.1f, 0.4f, -258.3f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #19
        { { 7.2f, 0.4f, -259.3f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #20
        { { 6.0f, 0.4f, -257.9f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #21
        { { 5.8f, 0.4f, -259.4f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #22
        { { 5.7f, 0.4f, -261.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #23
        { { 9.2f, 0.4f, -269.4f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #24
        { { 9.2f, 0.4f, -267.9f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #25
        { { 9.2f, 0.4f, -266.2f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #26
        { { 9.2f, 0.4f, -264.6f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #27
        { { 6.3f, 0.4f, -264.4f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #28
        { { 7.2f, 0.4f, -265.6f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #29
        { { 7.2f, 0.4f, -265.6f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #30
        { { 8.4f, 0.4f, -266.8f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #31
        { { 7.5f, 0.4f, -267.7f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #32
        { { 6.5f, 0.4f, -268.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #33
        { { 9.1f, 0.4f, -275.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #34
        { { 7.9f, 0.4f, -273.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #35
        { { 6.6f, 0.4f, -273.5f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #36
        { { 6.6f, 0.4f, -275.3f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #37
        { { 6.6f, 0.4f, -277.2f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #38
        { { 6.6f, 0.4f, -279.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },
        // Item Heal #39
        { { 7.7f, 0.4f, -279.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Heal },

        // ------------------------------------------
        // INVINCIBLE LIST
        // ------------------------------------------
        // Item Invincible #1
        { { 8.1f, 0.4f, -389.1f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Invincible },
        // Item Invincible #2
        { { -3.6f, 0.4f, -432.6f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Invincible },
        // Item Invincible #3
        { { 19.0f, 0.4f, -432.6f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }, ItemType::Invincible },
    };
}

class ItemManager
{
public:
    ItemManager();
    ~ItemManager();

    void Initialize(ID3D11Device* device);
    void Update(float elapsedTime, Camera* camera);
    void Render(ModelRenderer* renderer);
    void RenderDebug(ShapeRenderer* renderer);

    // Spawning
    void AddItem(ItemType type); // For GUI Button
    void ResetAllAnimations();
    void SpawnItem(const ItemSpawnData& data);
    void SpawnHealAt(const DirectX::XMFLOAT3& position);

    std::vector<std::unique_ptr<Item>>& GetItems() { return m_items; }

    // GUI Tools
    void SetHighlight(int index) { m_debugHighlightIndex = index; }
    int GetHighlight() const { return m_debugHighlightIndex; }

private:
    std::vector<std::unique_ptr<Item>> m_items;
    ID3D11Device* m_deviceRef = nullptr;
    int m_debugHighlightIndex = -1;
};