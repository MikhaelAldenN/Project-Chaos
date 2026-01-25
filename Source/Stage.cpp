#include "Stage.h"

using namespace DirectX;

std::vector<DebugWallData> Stage::SegmentLongWall(const DebugWallData& wall)
{
    static const float MAX_SEGMENT_LENGTH = 1.0f;
    static const float ASPECT_RATIO_LIMIT = 0.5f;

    std::vector<DebugWallData> segments;

    float halfExtentX = wall.Scale.x;
    float halfExtentZ = wall.Scale.z;

    float longestHalfExtent = (std::max)(halfExtentX, halfExtentZ);
    float shortestHalfExtent = (std::min)(halfExtentX, halfExtentZ);

    float fullLength = longestHalfExtent * 2.0f;
    float aspectRatio = fullLength / (shortestHalfExtent * 2.0f);

    bool needsSegmentation = (aspectRatio > ASPECT_RATIO_LIMIT) ||
        (fullLength > MAX_SEGMENT_LENGTH);

    if (!needsSegmentation) {
        segments.push_back(wall);
        return segments;
    }

    bool segmentX = (halfExtentX > halfExtentZ);
    float originalHalfExtent = segmentX ? halfExtentX : halfExtentZ;
    float originalFullLength = originalHalfExtent * 2.0f;

    int numSegments = (int)std::ceil(originalFullLength / MAX_SEGMENT_LENGTH);
    float newFullSegmentLength = originalFullLength / numSegments;
    float newHalfSegmentLength = newFullSegmentLength * 0.5f;

    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );

    for (int i = 0; i < numSegments; i++)
    {
        DebugWallData segment = wall;

        if (segmentX) {
            segment.Scale.x = newHalfSegmentLength;
        }
        else {
            segment.Scale.z = newHalfSegmentLength;
        }

        float localOffset = -originalHalfExtent + (i * newFullSegmentLength) + newHalfSegmentLength;

        XMVECTOR vLocalOffset = segmentX ?
            XMVectorSet(localOffset, 0, 0, 0) :
            XMVectorSet(0, 0, localOffset, 0);

        XMVECTOR vWorldOffset = XMVector3TransformNormal(vLocalOffset, matRot);

        XMFLOAT3 worldOffset;
        XMStoreFloat3(&worldOffset, vWorldOffset);

        segment.Position.x = wall.Position.x + worldOffset.x;
        segment.Position.y = wall.Position.y + worldOffset.y;
        segment.Position.z = wall.Position.z + worldOffset.z;

        segments.push_back(segment);
    }

    return segments;
}

Stage::Stage(ID3D11Device* device)
{
    // Load the model
    model = std::make_shared<Model>(device, StageConfig::MODEL_PATH);

    // Apply defaults
    position = StageConfig::DEFAULT_POS;
    rotation = StageConfig::DEFAULT_ROT;
    scale = StageConfig::DEFAULT_SCALE;
    color = StageConfig::DEFAULT_COLOR;

    // Initialize Debug Objects
    m_debugWalls.clear();
    for (const auto& originalWall : StageConfig::DEBUG_WALLS)
    {
        auto segments = SegmentLongWall(originalWall);
        m_debugWalls.insert(m_debugWalls.end(), segments.begin(), segments.end());
    }
    m_linesVoid = StageConfig::DEBUG_LINES_VOID;
    m_linesDisable = StageConfig::DEBUG_LINES_DISABLE;
    m_linesEnable = StageConfig::DEBUG_LINES_ENABLE;

    RebuildSpatialGrid();
    UpdateTransform();
}

void Stage::RebuildSpatialGrid()
{
    m_spatialGrid.Clear();

    for (size_t i = 0; i < m_debugWalls.size(); ++i)
    {
        const auto& wall = m_debugWalls[i];

        // Calculate bounding radius (conservative estimate)
        float radiusX = wall.Scale.x;
        float radiusZ = wall.Scale.z;
        float boundingRadius = std::sqrt(radiusX * radiusX + radiusZ * radiusZ);

        // Insert into grid
        m_spatialGrid.Insert(wall.Position, boundingRadius, i);
    }
}

void Stage::UpdateTransform()
{
    if (!model) return;

    // Calculate World Matrix: Scale * Rotation * Translation
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );
    XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);

    XMMATRIX world = S * R * T;

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);

    // Apply to the model's root nodes
    model->UpdateTransform(worldFloat);
}

void Stage::Render(ModelRenderer* renderer)
{
    if (!model || !renderer) return;
    renderer->Draw(ShaderId::Phong, model, color);
}

void Stage::RenderDebug(ShapeRenderer* shapeRenderer, PrimitiveRenderer* primRenderer)
{
    // Draw Debug Walls (GREEN BOXES)
    if (shapeRenderer)
    {
        DirectX::XMFLOAT4 wallColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        for (const auto& wall : m_debugWalls)
        {
            DirectX::XMFLOAT3 rotRadians = {
                XMConvertToRadians(wall.Rotation.x),
                XMConvertToRadians(wall.Rotation.y),
                XMConvertToRadians(wall.Rotation.z)
            };
            shapeRenderer->DrawBox(wall.Position, rotRadians, wall.Scale, wallColor);
        }
    }

    // Draw Debug Lines
    if (primRenderer)
    {
        auto DrawLineList = [&](const std::vector<DebugLineData>& lines, DirectX::XMFLOAT4 defaultColor, DebugLineType listType)
            {
                for (int i = 0; i < lines.size(); ++i)
                {
                    const auto& line = lines[i];
                    DirectX::XMFLOAT4 finalColor = defaultColor;

                    if (m_highlightState.index == i && m_highlightState.type == listType)
                    {
                        finalColor = { 1.0f, 1.0f, 0.0f, 1.0f }; // Bright Yellow Highlight
                    }

                    XMMATRIX T = XMMatrixTranslation(line.Position.x, line.Position.y, line.Position.z);
                    XMMATRIX R = XMMatrixRotationRollPitchYaw(
                        XMConvertToRadians(line.Rotation.x),
                        XMConvertToRadians(line.Rotation.y),
                        XMConvertToRadians(line.Rotation.z)
                    );

                    float halfLen = line.Scale.x * 0.5f;
                    XMVECTOR p0 = XMVectorSet(-halfLen, 0, 0, 1);
                    XMVECTOR p1 = XMVectorSet(halfLen, 0, 0, 1);

                    XMMATRIX W = R * T;
                    p0 = XMVector3TransformCoord(p0, W);
                    p1 = XMVector3TransformCoord(p1, W);

                    XMFLOAT3 v0, v1;
                    XMStoreFloat3(&v0, p0);
                    XMStoreFloat3(&v1, p1);

                    primRenderer->AddVertex(v0, finalColor);
                    primRenderer->AddVertex(v1, finalColor);
                }
            };

        // Draw Lists with their Type identifier
        DrawLineList(m_linesVoid, { 0.0f, 1.0f, 1.0f, 1.0f }, DebugLineType::Void);         // Cyan
        DrawLineList(m_linesDisable, { 1.0f, 0.0f, 0.0f, 1.0f }, DebugLineType::Disable);   // Red
        DrawLineList(m_linesEnable, { 0.0f, 1.0f, 0.0f, 1.0f }, DebugLineType::Enable);     // Green
    }
}

void Stage::AddDebugWall()
{
    DebugWallData newWall;

    if (!m_debugWalls.empty())
    {
        newWall = m_debugWalls.back();
    }
    else
    {
        // Fallback default if list is empty
        newWall.Position = { 0.0f, 0.0f, 0.0f };
        newWall.Rotation = { 0.0f, 0.0f, 0.0f };
        newWall.Scale = StageConfig::WALL_DEFAULT_SCALE;
    }

    m_debugWalls.push_back(newWall);
}

void Stage::AddDebugLine(DebugLineType type)
{
    DebugLineData newLine;
    newLine.Position = { 0,0,0 };
    newLine.Rotation = { 0,0,0 };
    newLine.Scale = StageConfig::LINE_DEFAULT_SCALE;

    // Determine which list to add to
    std::vector<DebugLineData>* targetList = nullptr;

    switch (type)
    {
    case DebugLineType::Void:
        targetList = &m_linesVoid;
        break;
    case DebugLineType::Disable:
        targetList = &m_linesDisable;
        break;
    case DebugLineType::Enable:
        targetList = &m_linesEnable;
        break;
    }

    if (targetList)
    {
        if (!targetList->empty()) newLine = targetList->back();
        targetList->push_back(newLine);
    }
}
