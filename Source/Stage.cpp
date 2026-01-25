#include "Stage.h"

using namespace DirectX;

std::vector<DebugWallData> Stage::SegmentLongWall(const DebugWallData& wall)
{
    static const float MAX_SEGMENT_LENGTH = 1.0f;
    static const float ASPECT_RATIO_LIMIT = 0.5f;

    std::vector<DebugWallData> segments;

    // ? CRITICAL: Your Scale values are HALF-EXTENTS, not full size
    // So the actual full length is Scale * 2
    float halfExtentX = wall.Scale.x;
    float halfExtentZ = wall.Scale.z;

    float longestHalfExtent = (std::max)(halfExtentX, halfExtentZ);
    float shortestHalfExtent = (std::min)(halfExtentX, halfExtentZ);

    // Full length for comparison
    float fullLength = longestHalfExtent * 2.0f;
    float aspectRatio = fullLength / (shortestHalfExtent * 2.0f);

    // Check if segmentation needed (using FULL length)
    bool needsSegmentation = (aspectRatio > ASPECT_RATIO_LIMIT) ||
        (fullLength > MAX_SEGMENT_LENGTH);

    if (!needsSegmentation) {
        segments.push_back(wall);
        return segments;
    }

    // Determine segmentation axis
    bool segmentX = (halfExtentX > halfExtentZ);
    float originalHalfExtent = segmentX ? halfExtentX : halfExtentZ;
    float originalFullLength = originalHalfExtent * 2.0f;

    // Calculate number of segments based on FULL length
    int numSegments = (int)std::ceil(originalFullLength / MAX_SEGMENT_LENGTH);
    float newFullSegmentLength = originalFullLength / numSegments;
    float newHalfSegmentLength = newFullSegmentLength * 0.5f;

    // Create rotation matrix for offset calculation
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(wall.Rotation.x),
        XMConvertToRadians(wall.Rotation.y),
        XMConvertToRadians(wall.Rotation.z)
    );

    // Generate segments
    for (int i = 0; i < numSegments; i++)
    {
        DebugWallData segment = wall;

        // ? Update scale (store as HALF-EXTENT)
        if (segmentX) {
            segment.Scale.x = newHalfSegmentLength;
        }
        else {
            segment.Scale.z = newHalfSegmentLength;
        }

        // ? Calculate offset from wall center
        // Start at -originalHalfExtent, place segment centers at regular intervals
        float localOffset = -originalHalfExtent + (i * newFullSegmentLength) + newHalfSegmentLength;

        // Apply offset in local space, then rotate to world space
        XMVECTOR vLocalOffset = segmentX ?
            XMVectorSet(localOffset, 0, 0, 0) :
            XMVectorSet(0, 0, localOffset, 0);

        XMVECTOR vWorldOffset = XMVector3TransformNormal(vLocalOffset, matRot);

        XMFLOAT3 worldOffset;
        XMStoreFloat3(&worldOffset, vWorldOffset);

        // Update position
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
    m_debugLines = StageConfig::DEBUG_LINES;

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
    // 1. Draw Debug Walls (GREEN BOXES)
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

    // 2. Draw Debug Lines 
    if (primRenderer)
    {
        DirectX::XMFLOAT4 lineColor = { 0.0f, 1.0f, 1.0f, 1.0f }; // Cyan

        for (const auto& line : m_debugLines)
        {
            // Build Matrix for Line
            XMMATRIX T = XMMatrixTranslation(line.Position.x, line.Position.y, line.Position.z);
            XMMATRIX R = XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(line.Rotation.x),
                XMConvertToRadians(line.Rotation.y),
                XMConvertToRadians(line.Rotation.z)
            );

            float halfLen = line.Scale.x * 0.5f;
            XMVECTOR p0 = XMVectorSet(-halfLen, 0, 0, 1);
            XMVECTOR p1 = XMVectorSet(halfLen, 0, 0, 1);

            // Transform points to world space
            XMMATRIX W = R * T;
            p0 = XMVector3TransformCoord(p0, W);
            p1 = XMVector3TransformCoord(p1, W);

            XMFLOAT3 v0, v1;
            XMStoreFloat3(&v0, p0);
            XMStoreFloat3(&v1, p1);

            primRenderer->AddVertex(v0, lineColor);
            primRenderer->AddVertex(v1, lineColor);
        }
    }
}

void Stage::AddDebugWall()
{
    DebugWallData newWall;

    // If walls already exist, copy the last one's data
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

void Stage::AddDebugLine()
{
    DebugLineData newLine;
    if (!m_debugLines.empty()) newLine = m_debugLines.back();
    else {
        newLine.Position = { 0,0,0 };
        newLine.Rotation = { 0,0,0 };
        newLine.Scale = StageConfig::LINE_DEFAULT_SCALE; 
    }
    m_debugLines.push_back(newLine);
}
