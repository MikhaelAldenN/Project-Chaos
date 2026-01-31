#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "System/ModelRenderer.h"
#include "System/ShapeRenderer.h" 
#include "System/PrimitiveRenderer.h"

// ==========================================
// STAGE CONFIGURATION 
// ==========================================
struct DebugWallData {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation;
    DirectX::XMFLOAT3 Scale;
    float WorldRadius;
};

struct DebugLineData {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation;
    DirectX::XMFLOAT3 Scale;
};

struct SpatialHashGrid
{
    // ==========================================
    // OPTIMIZATION: Fixed Grid Settings
    // ==========================================
    static constexpr float CELL_SIZE = 10.0f;   // Larger cells = faster for large objects

    // Grid Bounds (Based on your wall coordinates)
    // X Range: Covers -100 to +100
    // Z Range: Covers -900 to +100
    static constexpr int COLS = 20;
    static constexpr int ROWS = 100;
    static constexpr float OFFSET_X = 100.0f; 
    static constexpr float OFFSET_Z = 900.0f; 

    std::vector<size_t> cells[COLS * ROWS];

    void Clear()
    {
        for (auto& cell : cells)
        {
            cell.clear();
        }
    }

    void Insert(const DirectX::XMFLOAT3& center, float radius, size_t index)
    {
        // Convert World Pos -> Grid Indices
        int minX = (int)((center.x - radius + OFFSET_X) / CELL_SIZE);
        int maxX = (int)((center.x + radius + OFFSET_X) / CELL_SIZE);
        int minZ = (int)((center.z - radius + OFFSET_Z) / CELL_SIZE);
        int maxZ = (int)((center.z + radius + OFFSET_Z) / CELL_SIZE);

        // Clamp to ensure we don't crash
        minX = (std::max)(0, (std::min)(minX, COLS - 1));
        maxX = (std::max)(0, (std::min)(maxX, COLS - 1));
        minZ = (std::max)(0, (std::min)(minZ, ROWS - 1));
        maxZ = (std::max)(0, (std::min)(maxZ, ROWS - 1));

        // Fill buckets
        for (int z = minZ; z <= maxZ; ++z)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                cells[z * COLS + x].push_back(index);
            }
        }
    }

    // Standard Query
    std::vector<size_t> QueryRadius(const DirectX::XMFLOAT3& center, float radius) const
    {
        int minX = (int)((center.x - radius + OFFSET_X) / CELL_SIZE);
        int maxX = (int)((center.x + radius + OFFSET_X) / CELL_SIZE);
        int minZ = (int)((center.z - radius + OFFSET_Z) / CELL_SIZE);
        int maxZ = (int)((center.z + radius + OFFSET_Z) / CELL_SIZE);

        minX = (std::max)(0, (std::min)(minX, COLS - 1));
        maxX = (std::max)(0, (std::min)(maxX, COLS - 1));
        minZ = (std::max)(0, (std::min)(minZ, ROWS - 1));
        maxZ = (std::max)(0, (std::min)(maxZ, ROWS - 1));

        std::vector<size_t> results;
        // Optimization: Reserve rough amount to avoid small reallocs
        results.reserve(16);

        for (int z = minZ; z <= maxZ; ++z)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const auto& cell = cells[z * COLS + x];
                results.insert(results.end(), cell.begin(), cell.end());
            }
        }

        std::sort(results.begin(), results.end());
        auto last = std::unique(results.begin(), results.end());
        results.erase(last, results.end());

        return results;
    }
};

namespace StageConfig
{
    static const char* MODEL_PATH = "Data/Model/Stage/StageBeyondBreaker.glb";
    static const DirectX::XMFLOAT3 DEFAULT_POS = { 0.0f, -0.3f, -30.0f };
    static const DirectX::XMFLOAT3 DEFAULT_ROT = { 0.0f, 270.0f, 0.0f };
    static const DirectX::XMFLOAT3 DEFAULT_SCALE = { 900.0f, 900.0f, 900.0f };
    static const DirectX::XMFLOAT4 DEFAULT_COLOR = { 0.275f, 0.275f, 0.275f, 1.0f };

    static const DirectX::XMFLOAT3 WALL_DEFAULT_SCALE = { 1.0f, 1.0f, 1.0f };
    static const DirectX::XMFLOAT3 LINE_DEFAULT_SCALE = { 10.0f, 0.0f, 0.0f };

    // =========================================================
    // DEBUG WALL CONFIGURATION
    // =========================================================
    static const std::vector<DebugWallData> DEBUG_WALLS = 
    {
        // Wall 1
        { {5.4,0.2,-42.3}, {0,-0.2,0}, {0.35,0.45,18.45} },
        // Wall 2
        { {-5.3,0.2,-42.3}, {0,-0.2,0}, {0.35,0.45,18.5} },
        // Wall 3
        { {0,0.2,-23.5}, {0,90,0}, {0.35,0.45,5.45} },
        // Wall 4
        { {4.2,0.2,-62}, {0,49.8,0}, {0.35,0.45,1.5} },
        // Wall 5
        { {-4,0.2,-62}, {0,-49.8,0}, {0.35,0.45,1.5} },
        // Wall 6
        { {3.1,0.2,-76.3}, {0,0.2,0}, {0.35,0.45,13.6} },
        // Wall 7
        { {-3,0.2,-66.5}, {0,0.2,0}, {0.35,0.45,3.75} },
        // Wall 8
        { {-3,0.2,-86.7}, {0,0.2,0}, {0.35,0.45,0.85} },
        // Wall 9
        { {-7.2,0.2,-91.7}, {0,44.7,0}, {0.35,0.45,6.2} },
        // Wall 10
        { {-10.7,0.2,-101.8}, {0.0,0.2,0.0}, {0.35,0.45,6.4} },
        // Wall 11
        { {-8.3,0.2,-110}, {0,-44.8,0}, {0.35,0.45,3.85} },
        // Wall 12
        { {-2.2,0.2,-108}, {0,-44.8,0}, {0.35,0.45,3.6} },
        // Wall 13
        { {0.5,0.2,-118}, {0,0,0}, {0.35,0.45,7.15} },
        // Wall 14
        { {-5.6,0.2,-119}, {0,0,0}, {0.35,0.45,6.05} },
        // Wall 15
        { {-6.7,0.2,-125.8}, {0,50.8,0}, {0.35,0.45,1.55} },
        // Wall 16
        { {1.6,0.2,-125.5}, {0,-50.8,0}, {0.35,0.45,1.55} },
        // Wall 17
        { {2.8,0.2,-143}, {0,0,0}, {0.35,0.45,16.3} },
        // Wall 18
        { {-8,0.2,-144}, {0,0,0}, {0.35,0.45,16.9} },
        // Wall 19
        { {-6.9,0.2,-168}, {0,-7.4,0}, {0.35,0.45,8.35} },
        // Wall 20
        { {4.9,0.2,-175}, {0,-7.4,0}, {0.35,0.45,16.3} },
        // Wall 21
        { {11.1,0.2,-207}, {0,-14.9,0}, {0.35,0.45,16.5} },
        // Wall 22
        { {2.5,0.2,-216}, {0,-15.5,0}, {0.35,0.45,7.85} },
        // Wall 23
        { {4.6,0.2,-251}, {0,0,0}, {0.35,0.45,27.5} },
        // Wall 24
        { {12.9,0.2,-258.6}, {0,51.2,0}, {0.35,0.45,3.1} },
        // Wall 25
        { {10.6,0.2,-269}, {0,0,0}, {0.35,0.45,9} },
        // Wall 26
        { {12.9,0.2,-280}, {0,-50.5,0}, {0.35,0.45,3.15} },
        // Wall 27
        { {2.2,0.2,-280}, {0,50.5,0}, {0.35,0.45,3.35} },
        // Wall 28
        { {-0.3,0.2,-322}, {0,0,0}, {0.35,0.45,39.2} },
        // Wall 29
        { {15.3,0.2,-322}, {0,0,0}, {0.35,0.45,39.7} },
        // Wall 30
        { {18.8,0.2,-364}, {0,-48.7,0}, {0.35,0.45,4.85} },
        // Wall 31
        { {-3.7,0.2,-364}, {0,47.2,0}, {0.35,0.45,4.85} },
        // Wall 32
        { {-6.6,0.2,-418}, {0,0,0}, {0.35,0.45,52.1} },
        // Wall 33
        { {21.8,0.2,-419}, {0,0,0}, {0.35,0.45,54.3} },
        // Wall 34
        { {0.5,0.2,-587}, {0,0,0}, {0.35,0.45,12.9} },
        // Wall 35
        { {-28.2,0.2,-581}, {0,0,0}, {0.35,0.45,18.9} },
        // Wall 36
        { {-25.1,0.2,-602}, {0,-51,0}, {0.35,0.45,4.8} },
        // Wall 37
        { {-2.5,0.2,-602}, {0,51,0}, {0.35,0.45,4.75} },
        // Wall 38
        { {-6.1,0.2,-615.4}, {0,0,0}, {0.35,0.45,10.6} },
        // Wall 39
        { {-21.5,0.2,-615.4}, {0,0,0}, {0.35,0.45,10.6} },
        // Wall 40
        { {-20,0.2,-627.4}, {0,-47.7,0}, {0.35,0.45,2.05} },
        // Wall 41
        { {-7.1,0.2,-627}, {0,47.7,0}, {0.35,0.45,2.55} },
        // Wall 42
        { {-9,0.2,-644}, {0,0,0}, {0.35,0.45,15.5} },
        // Wall 43
        { {-18.6,0.2,-644}, {0,0,0}, {0.35,0.45,15.5} },
        // Wall 44
        { {-17.7,0.2,-660}, {0,-52.7,0}, {0.35,0.45,0.95} },
        // Wall 45
        { {-9.9,0.2,-660}, {0,52.7,0}, {0.35,0.45,0.95} },
        // Wall 46
        { {-10.5,0.2,-676}, {0,0,0}, {0.35,0.45,15.8} },
        // Wall 47
        { {-17.1,0.2,-676}, {0,0,0}, {0.35,0.45,15.8} },
        // Wall 48
        { {-24.5,0.2,-691.4}, {0,90,0}, {0.35,0.45,7.45} },
        // Wall 49
        { {-2.7,0.2,-691.4}, {0,90,0}, {0.35,0.45,7.45} },
        // Wall 50
        { {4.8,0.2,-710}, {0,0,0}, {0.35,0.45,19} },
        // Wall 51
        { {-32.3,0.2,-710}, {0,0,0}, {0.35,0.45,19} },
        // Wall 52
        { {-13.7,0.2,-728.6}, {0,90,0}, {0.35,0.45,19} },
    };

    // =========================================================
    // VOID LINES (Cyan) - Causes falling
    // =========================================================
    static const std::vector<DebugLineData> DEBUG_LINES_VOID =
    {
        // Line Void 1
        { {-3.25,0,-78.1}, {0,90,0}, {15.4,0,0} },
        // Line Void 2
        { {-0.45,0,-93.9}, {0,-45.9,0}, {11.2,0,0} },
        // Line Void 3
         { {-4.35,0,-101.6}, {0,-90,0}, {7.4,0,0} },
        // Line Void 4
        { {-5.15,0,-184.4}, {0,82.8,0}, {17.5,0,0} },
        // Line Void 5
        { {-2.85,0,-197.4}, {0,74.5,0}, {25,0,0} },
        // Line Void 6
        { {-14.95,0,-500.5}, {0,104.9,0}, {62,0,0} },
        // Line Void 7
        { {-13.25,0,-509.9}, {0,74.9,0}, {25.5,0,0} },
        // Line Void 8
        { {-21.55,0,-533.9}, {0,134.8,0}, {33.1,0,0} },
        // Line Void 9
        { {-22.95,0,-540.9}, {0,60.4,0}, {23.4,0,0} },
        // Line Void 10
        { {-22.85,0,-556.6}, {0,135.8,0}, {15.9,0,0} },
        // Line Void 11
        { {18.45,0,-487.5}, {0,-75,0}, {30.1,0,0} },
        // Line Void 12
        { {18.85,0,-518}, {0,-105.1,0}, {33.1,0,0} },
        // Line Void 13
        { {17.05,0,-536.2}, {0,-44.8,0}, {16.7,0,0} },
        // Line Void 14
        { {15.45,0,-549.7}, {0,-119.8,0}, {17.6,0,0} },
        // Line Void 15
        { {17.75,0,-556.9}, {0,-45.2,0}, {48.7,0,0} },
        // Line Void 16
        { {15.65,0,-238.6}, {0,-90,0}, {36.1,0,0} },
    };

    // =========================================================
    // DISABLE LINES (Red) - Disables Input/Mechanic
    // =========================================================
    static const std::vector<DebugLineData> DEBUG_LINES_DISABLE =
    {
        // Line Disable 1
        { {7.5,0,-260.7}, {0,0,0}, {5.5,0,0} },
        // Line Disable 2
        { {-13.9,0,-660.1}, {0,0,0}, {6,0,0} },
    };

    // =========================================================
    // ENABLE LINES (Green) - Enables Input/Mechanic
    // =========================================================
    static const std::vector<DebugLineData> DEBUG_LINES_ENABLE =
    {
        // Line Enable 1
        { {-2.6,0,-130.3}, {0,0,0}, {10,0,0} },
        // Line Enable 2
        { {7.4,0,-288.4}, {0,0,0}, {14.8,0,0} },
        // Line Enable 3
        { {-13.1,0,-691.4}, {0,0,0}, {35.5,0,0} },
    };

    // =========================================================
    // CHECKPOINT
    // =========================================================
    static const std::vector<DebugLineData> DEBUG_LINES_CHECKPOINT =
    {
        // Line CheckPoint 1
        { {0,0,-27}, {0,0,0}, {9.8,0,0} },
        // Line CheckPoint 2
        { {-2.6,0,-130.3}, {0,0,0}, {10,0,0} },
        // Line CheckPoint 3
        { {7.4,0,-288.4}, {0,0,0}, {14.8,0,0} },
    };
}

enum class DebugLineType { Void, Disable, Enable, Checkpoint };

class Stage
{
public:
    Stage(ID3D11Device* device);
    ~Stage() = default;

    void UpdateTransform();
    void Render(ModelRenderer* renderer);
    void RenderDebug(ShapeRenderer* shapeRenderer, PrimitiveRenderer* primRenderer);

    // Dynamic Debug Tools
    void AddDebugWall();
    void AddDebugLine(DebugLineType type);
    void SetLineHighlight(DebugLineType type, int index) { m_highlightState = { type, index }; }
    void SetWallHighlight(int index) { m_highlightWallIndex = index; }
    void ClearLineHighlight() { m_highlightState.index = -1; }
    void ClearWallHighlight() { m_highlightWallIndex = -1; }

    std::shared_ptr<Model> GetModel() const { return model; }
    const SpatialHashGrid& GetSpatialGrid() const { return m_spatialGrid; }

public:
    // Public variables for GUI editing
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation;
    DirectX::XMFLOAT3 scale;
    DirectX::XMFLOAT4 color;

    std::vector<DebugWallData> m_debugWalls;
    std::vector<DebugLineData> m_linesVoid;
    std::vector<DebugLineData> m_linesDisable;
    std::vector<DebugLineData> m_linesEnable;
    std::vector<DebugLineData> m_linesCheckpoint;

private:
    struct HighlightData {
        DebugLineType type = DebugLineType::Void; 
        int index = -1; 
    } m_highlightState;
    int m_highlightWallIndex = -1;

    void RebuildSpatialGrid();

    SpatialHashGrid m_spatialGrid;
    std::shared_ptr<Model> model;
};