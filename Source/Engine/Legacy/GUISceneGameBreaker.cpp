#include "Enemy.h"
#include "CameraController.h"
#include "CinematicDirector.h"
#include "GUISceneGameBreaker.h"
#include "SceneGame.h" 
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdio>
#include <unordered_map>

using namespace DirectX;

static DirectX::XMFLOAT3 GUICalculateRotation(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target)
{
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vTarget = XMLoadFloat3(&target);
    XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
    vDir = XMVector3Normalize(vDir);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);

    float pitch = asinf(-dir.y);
    float yaw = atan2f(dir.x, dir.z);

    return { pitch, yaw, 0.0f };
}

std::string FloatToString(float f)
{
    char buf[64];
    snprintf(buf, 64, "%.6g", f);
    std::string s(buf);

    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
    {
        s += ".0";
    }
    return s + "f";
}

std::string GenerateEnemyCopyString(Enemy* e, int index, const char* commentName, const DirectX::XMFLOAT3& currentPos)
{
    char buffer[512];

    XMFLOAT3 pos = currentPos;
    XMFLOAT3 rot = e->GetOriginalRotation();

    std::string rotStr = "{ " + FloatToString(rot.x) + ", " + FloatToString(rot.y) + ", " + FloatToString(rot.z) + " }";
    if (abs(rot.y - 0.0f) < 0.01f) rotStr = "Rot::Backward";
    else if (abs(rot.y - DirectX::XM_PI) < 0.01f) rotStr = "Rot::Forward";
    else if (abs(rot.y - DirectX::XM_PIDIV2) < 0.01f) rotStr = "Rot::Left";
    else if (abs(rot.y + DirectX::XM_PIDIV2) < 0.01f) rotStr = "Rot::Right";

    std::string typeStr = (e->GetType() == EnemyType::Paddle) ? "EnemyType::Paddle" : "EnemyType::Ball";
    std::string colorStr = (e->GetType() == EnemyType::Paddle) ? "Blue" : "PaleYellow";

    std::string attackStr = "AttackType::None";
    std::string extraParams = "";

    switch (e->GetAttackType())
    {
    case AttackType::Static: attackStr = "AttackType::Static"; break;
    case AttackType::Tracking: attackStr = "AttackType::Tracking"; break;
    case AttackType::TrackingHorizontal:
        attackStr = "AttackType::TrackingHorizontal";
        extraParams = ", MoveDir::" + std::string(e->GetMoveDir() == MoveDir::Left ? "Left" : "Right");
        extraParams += ", " + FloatToString(e->GetMinX()) + ", " + FloatToString(e->GetMaxX());
        break;
    case AttackType::TrackingRandom:
        attackStr = "AttackType::TrackingRandom";
        extraParams = ", MoveDir::None";
        extraParams += ", " + FloatToString(e->GetMinX()) + ", " + FloatToString(e->GetMaxX());
        extraParams += ", " + FloatToString(e->GetMinZ()) + ", " + FloatToString(e->GetMaxZ());
        break;
    }

    std::string posStr = "{ " + FloatToString(pos.x) + ", " + FloatToString(pos.y) + ", " + FloatToString(pos.z) + " }";

    snprintf(buffer, sizeof(buffer),
        "// %s %d\n{ %s, %s, %s, %s, %s%s },",
        commentName, index, posStr.c_str(), rotStr.c_str(), colorStr.c_str(), typeStr.c_str(), attackStr.c_str(), extraParams.c_str());

    return std::string(buffer);
}

// Renamed parameter to SceneGame*
void GameBreakerGUI::Draw(SceneGame* scene)
{
    CameraController::Instance().DrawDebugGUI();

    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Inspector", nullptr))
    {
        if (ImGui::BeginTabBar("InspectorTabs"))
        {
            if (ImGui::BeginTabItem("Camera Info"))
            {
                DrawCameraTab(scene);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Post-Process & FX"))
            {
                DrawPostProcessTab(scene);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Object Color"))
            {
                DrawObjectColorTab(scene);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Object Transform"))
            {
                DrawObjectTransformTab(scene);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// Renamed parameter to SceneGame*
void GameBreakerGUI::DrawCameraTab(SceneGame* scene)
{
    auto& camCtrl = CameraController::Instance();
    auto mainCam = scene->GetMainCamera();

    if (!scene->m_director)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: CinematicDirector is NULL!");
        return;
    }

    auto& scenarios = scene->m_director->GetScenarios();
    static bool isSequencePlaying = false;
    const bool ctrlIsSequencing = camCtrl.IsSequencing();
    if (!ctrlIsSequencing) isSequencePlaying = false;

    const float availWidth = ImGui::GetContentRegionAvail().x;
    if (isSequencePlaying)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP SEQUENCE", ImVec2(availWidth, 40)))
        {
            camCtrl.StopSequence();
            isSequencePlaying = false;
            if (!scenarios.empty()) {
                camCtrl.SetFixedSetting(scenarios[0].StartPos);
                camCtrl.SetTarget(scenarios[0].StartLookAt);
            }
        }
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        if (ImGui::Button("PLAY SEQUENCE", ImVec2(availWidth, 40)))
        {
            std::vector<CameraKeyframe> sequence;
            for (const auto& point : scenarios)
            {
                CameraKeyframe key;
                key.isJumpCut = true;
                key.StartPosition = point.StartPos;
                key.StartRotation = GUICalculateRotation(point.StartPos, point.StartLookAt);
                key.TargetPosition = point.EndPos;
                key.TargetRotation = GUICalculateRotation(point.EndPos, point.EndLookAt);
                key.Duration = point.Duration;
                key.Easing = point.Easing;
                sequence.push_back(key);
            }
            camCtrl.PlaySequence(sequence, false);
            isSequencePlaying = true;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "SHOT LIST / KEYFRAMES");

    for (int i = 0; i < scenarios.size(); ++i)
    {
        auto& pt = scenarios[i];
        ImGui::PushID(i);
        char headerName[64];
        snprintf(headerName, 64, "#%d - %s", i + 1, pt.Name.c_str());

        if (ImGui::CollapsingHeader(headerName, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::DragFloat("Duration", &pt.Duration, 0.1f, 0.1f, 20.0f);

            int currentEaseIdx = 0;
            if (pt.Easing == EasingType::Linear)        currentEaseIdx = 0;
            else if (pt.Easing == EasingType::EaseInCubic)   currentEaseIdx = 1;
            else if (pt.Easing == EasingType::EaseOutCubic)  currentEaseIdx = 2;
            else if (pt.Easing == EasingType::SmoothStep)    currentEaseIdx = 3;

            const char* easeItems[] = { "Linear", "Ease In", "Ease Out", "Smooth Step" };
            if (ImGui::Combo("Motion", &currentEaseIdx, easeItems, IM_ARRAYSIZE(easeItems)))
            {
                switch (currentEaseIdx) {
                case 0: pt.Easing = EasingType::Linear; break;
                case 1: pt.Easing = EasingType::EaseInCubic; break;
                case 2: pt.Easing = EasingType::EaseOutCubic; break;
                case 3: pt.Easing = EasingType::SmoothStep; break;
                }
            }

            ImGui::Separator();
            ImGui::Columns(2, nullptr, true);

            // === START ===
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ A ] START");
            if (ImGui::Button("View##S")) {
                camCtrl.StopSequence();
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                camCtrl.SetFixedSetting(pt.StartPos);
                camCtrl.SetTarget(pt.StartLookAt);
                if (mainCam) { mainCam->SetPosition(pt.StartPos); mainCam->LookAt(pt.StartLookAt); }
            }
            if (ImGui::Button("Set to Current##S")) {
                if (mainCam) {
                    pt.StartPos = mainCam->GetPosition();
                    XMFLOAT3 rot = mainCam->GetRotation();
                    XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
                    XMStoreFloat3(&pt.StartLookAt, XMVectorAdd(XMLoadFloat3(&pt.StartPos), vDir));
                }
            }
            if (ImGui::TreeNode("Edit Coords##S")) {
                bool changed = false;
                changed |= ImGui::DragFloat3("Pos##S", &pt.StartPos.x, 0.1f);
                changed |= ImGui::DragFloat3("Look##S", &pt.StartLookAt.x, 0.1f);
                if (changed && mainCam) { mainCam->SetPosition(pt.StartPos); mainCam->LookAt(pt.StartLookAt); }
                ImGui::TreePop();
            }

            ImGui::NextColumn();

            // === END ===
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[ B ] END");
            if (ImGui::Button("View##E")) {
                camCtrl.StopSequence();
                camCtrl.SetControlMode(CameraControlMode::FixedStatic);
                camCtrl.SetFixedSetting(pt.EndPos);
                camCtrl.SetTarget(pt.EndLookAt);
                if (mainCam) { mainCam->SetPosition(pt.EndPos); mainCam->LookAt(pt.EndLookAt); }
            }
            if (ImGui::Button("Set to Current##E")) {
                if (mainCam) {
                    pt.EndPos = mainCam->GetPosition();
                    XMFLOAT3 rot = mainCam->GetRotation();
                    XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
                    XMStoreFloat3(&pt.EndLookAt, XMVectorAdd(XMLoadFloat3(&pt.EndPos), vDir));
                }
            }
            if (ImGui::TreeNode("Edit Coords##E")) {
                bool changed = false;
                changed |= ImGui::DragFloat3("Pos##E", &pt.EndPos.x, 0.1f);
                changed |= ImGui::DragFloat3("Look##E", &pt.EndLookAt.x, 0.1f);
                if (changed && mainCam) { mainCam->SetPosition(pt.EndPos); mainCam->LookAt(pt.EndLookAt); }
                ImGui::TreePop();
            }

            ImGui::Columns(1);
            ImGui::Unindent();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button("+ Add New Shot", ImVec2(-1, 30)))
    {
        SceneCameraPoint newPt;
        newPt.Name = "New Shot";
        newPt.Duration = 2.0f;
        newPt.Easing = EasingType::SmoothStep;
        if (mainCam) {
            newPt.StartPos = mainCam->GetPosition();
            newPt.EndPos = mainCam->GetPosition();
            XMFLOAT3 rot = mainCam->GetRotation();
            XMVECTOR vDir = XMVector3TransformNormal(XMVectorSet(0, 0, 10.0f, 0), XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z));
            XMStoreFloat3(&newPt.StartLookAt, XMVectorAdd(XMLoadFloat3(&newPt.StartPos), vDir));
            newPt.EndLookAt = newPt.StartLookAt;
        }
        scenarios.push_back(newPt);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "LIGHTING SETTINGS");

    DirectionalLight light = scene->m_lightManager.GetDirectionalLight();
    bool lightChanged = false;

    ImGui::Text("Light Direction");
    if (ImGui::DragFloat3("##LightDir", &light.direction.x, 0.01f, -1.0f, 1.0f))
    {
        lightChanged = true;
    }

    ImGui::Text("Light Color");
    if (ImGui::ColorEdit3("##LightColor", &light.color.x))
    {
        lightChanged = true;
    }

    if (lightChanged)
    {
        XMVECTOR vDir = XMLoadFloat3(&light.direction);
        vDir = XMVector3Normalize(vDir);
        XMStoreFloat3(&light.direction, vDir);
        scene->m_lightManager.SetDirectionalLight(light);
    }
}

// Renamed parameter to SceneGame*
void GameBreakerGUI::DrawPostProcessTab(SceneGame* scene)
{
    ImGui::Spacing();

    if (scene->m_fxState.MasterEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Turn Off Filter", ImVec2(-1, 40))) scene->m_fxState.MasterEnabled = false;
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Turn On Filter", ImVec2(-1, 40))) scene->m_fxState.MasterEnabled = true;
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (!scene->m_fxState.MasterEnabled) {
        ImGui::TextDisabled("Post-processing pipeline is bypassed.");
        return;
    }

    // Fixed variable name: m_uberParams
    auto& uber = scene->m_uberParams;

    if (ImGui::CollapsingHeader("Vignette & Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Vignette", &scene->m_fxState.EnableVignette);
        if (!scene->m_fxState.EnableVignette) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::ColorEdit3("Tint Color", &uber.color.x);
        ImGui::SliderFloat("Intensity", &uber.intensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Smoothness", &uber.smoothness, 0.01f, 1.0f);
        ImGui::Checkbox("Rounded Mask", &uber.rounded);
        if (uber.rounded) ImGui::SliderFloat("Roundness", &uber.roundness, 0.0f, 1.0f);

        if (!scene->m_fxState.EnableVignette) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Lens", &scene->m_fxState.EnableLens);
        if (!scene->m_fxState.EnableLens) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::SliderFloat("Fisheye", &uber.distortion, -0.5f, 0.5f);
        ImGui::SliderFloat("Chroma Blur", &uber.blurStrength, 0.0f, 0.05f, "%.4f");
        ImGui::SliderFloat("Glitch Shake", &uber.glitchStrength, 0.0f, 1.0f);

        if (!scene->m_fxState.EnableLens) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("CRT Monitor Effect"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: CRT", &scene->m_fxState.EnableCRT);
        if (!scene->m_fxState.EnableCRT) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::SliderFloat("Scanline Opacity", &uber.scanlineStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Scanline Size", &uber.scanlineSize, 1.0f, 150.0f);
        ImGui::SliderFloat("Fine Density (Idle)", &scene->m_configFineDensity, 1.0f, 100.0f);
        ImGui::SliderFloat("Fine Density (Zoom)", &scene->m_configZoomDensity, 1.0f, 100.0f);

        if (!scene->m_fxState.EnableCRT) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Chromatic Aberration"))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Chromatic", &scene->m_fxState.EnableChromatic);

        if (!scene->m_fxState.EnableChromatic) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        // Fixed variable name: m_uberParams
        ImGui::SliderFloat("Intensity", &scene->m_uberParams.chromaticAberration, -0.02f, 0.02f, "%.5f");

        if (ImGui::Button("Reset CA")) scene->m_uberParams.chromaticAberration = 0.0f;

        if (!scene->m_fxState.EnableChromatic) ImGui::PopStyleVar();
        ImGui::Unindent();
    }
}

// Renamed parameter to SceneGame*
void GameBreakerGUI::DrawObjectColorTab(SceneGame* scene)
{
    ImGui::Spacing();

    if (scene->m_stage)
    {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "STAGE");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Stage", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##Stage", &scene->m_stage->color.x);
            ImGui::Unindent();
        }
        ImGui::Spacing();
    }

    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CHARACTER");
    ImGui::Separator();
    ImGui::Spacing();

    // Fixed variable name: m_player
    if (scene->m_player)
    {
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##Player", &scene->m_player->color.x);
            ImGui::Unindent();
        }
        ImGui::Spacing();
    }

    if (scene->m_enemyManager)
    {
        if (ImGui::CollapsingHeader("Enemies"))
        {
            ImGui::Indent();

            auto& enemies = scene->m_enemyManager->GetEnemies();
            auto DrawEnemyColor = [](Enemy* e, int index, const char* label)
                {
                    ImGui::PushID(index);
                    ImGui::ColorEdit4(label, &e->color.x);
                    ImGui::PopID();
                };

            int paddleCounter = 0;
            int ballCounter = 0;

            for (size_t i = 0; i < enemies.size(); ++i)
            {
                Enemy* e = enemies[i].get();
                if (e->GetType() == EnemyType::Paddle)
                {
                    paddleCounter++;
                    char label[64];
                    snprintf(label, 64, "Enemy Paddle #%d", paddleCounter);
                    DrawEnemyColor(e, (int)i, label);
                }
            }

            for (size_t i = 0; i < enemies.size(); ++i)
            {
                Enemy* e = enemies[i].get();
                if (e->GetType() == EnemyType::Ball)
                {
                    ballCounter++;
                    char label[64];
                    snprintf(label, 64, "Enemy Ball #%d", ballCounter);
                    DrawEnemyColor(e, (int)i, label);
                }
            }
            ImGui::Unindent();
        }
        ImGui::Spacing();
    }

    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Items");
    ImGui::Separator();
    ImGui::Spacing();

    if (scene->m_itemManager)
    {
        if (ImGui::CollapsingHeader("Items"))
        {
            ImGui::Indent();
            auto& items = scene->m_itemManager->GetItems();
            int healCount = 0;

            for (int i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Heal)
                {
                    healCount++;
                    char label[64];
                    snprintf(label, 64, "Item Heal #%d", healCount);

                    ImGui::PushID(i);
                    ImGui::ColorEdit4(label, &items[i]->color.x);
                    ImGui::PopID();
                }
            }
            ImGui::Unindent();
        }
    }
}

// Renamed parameter to SceneGame*
void GameBreakerGUI::DrawObjectTransformTab(SceneGame* scene)
{
    ImGui::Spacing();
    ImGui::Text("Adjust 3D Object Transforms:");
    ImGui::Separator();

    if (scene->m_stage)
    {
        // 1. ORIGINAL STAGE TRANSFORM
        if (ImGui::CollapsingHeader("Stage Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "POSITION");
            ImGui::DragFloat3("XYZ##StagePos", &scene->m_stage->position.x, 0.1f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "ROTATION");
            ImGui::DragFloat3("Pitch/Yaw/Roll##StageRot", &scene->m_stage->rotation.x, 0.1f, -180.0f, 180.0f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SCALE");
            ImGui::DragFloat3("XYZ##StageScl", &scene->m_stage->scale.x, 0.01f, 0.1f, 100.0f);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Reset Transform", ImVec2(-1, 30)))
            {
                scene->m_stage->position = StageConfig::DEFAULT_POS;
                scene->m_stage->rotation = StageConfig::DEFAULT_ROT;
                scene->m_stage->scale = StageConfig::DEFAULT_SCALE;
            }
            ImGui::Unindent();
        }

        // 2. DEBUG WALL TRANSFORM (DYNAMIC)
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Debug Wall Transform", ImGuiTreeNodeFlags_None))
        {
            scene->m_stage->ClearWallHighlight();

            ImGui::Indent();
            ImGui::TextDisabled("Edit debug boxes for collision setup.");

            for (int i = 0; i < scene->m_stage->m_debugWalls.size(); ++i)
            {
                auto& wall = scene->m_stage->m_debugWalls[i];
                char label[32];
                snprintf(label, 32, "Wall #%d", i + 1);
                ImGui::PushID(i);
                if (ImGui::TreeNode(label))
                {
                    scene->m_stage->SetWallHighlight(i);

                    ImGui::DragFloat3("Pos", &wall.Position.x, 0.1f);
                    ImGui::DragFloat3("Rot", &wall.Rotation.x, 0.1f, -180.0f, 180.0f);
                    ImGui::DragFloat3("Scale", &wall.Scale.x, 0.05f, 0.0f, 100.0f);

                    if (ImGui::Button("Reset This Wall"))
                    {
                        if (i < StageConfig::DEBUG_WALLS.size()) {
                            wall = StageConfig::DEBUG_WALLS[i];
                        }
                        else {
                            wall.Position = { 0,0,0 };
                            wall.Rotation = { 0,0,0 };
                            wall.Scale = StageConfig::WALL_DEFAULT_SCALE;
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Copy Value"))
                    {
                        char buffer[256];
                        snprintf(buffer, sizeof(buffer),
                            "// Wall %d\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
                            i + 1,
                            wall.Position.x, wall.Position.y, wall.Position.z,
                            wall.Rotation.x, wall.Rotation.y, wall.Rotation.z,
                            wall.Scale.x, wall.Scale.y, wall.Scale.z
                        );
                        ImGui::SetClipboardText(buffer);
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("+ Add Wall Debug", ImVec2(-1, 30)))
            {
                scene->m_stage->AddDebugWall();
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Debug Line Transform", ImGuiTreeNodeFlags_None))
        {
            scene->m_stage->ClearLineHighlight();

            ImGui::Indent();
            ImGui::TextDisabled("Lines are X-Axis aligned. Scale.X = Length.");
            ImGui::TextDisabled("Yellow = Currently Editing");

            auto DrawLineCategory = [&](const char* categoryName, std::vector<DebugLineData>& lines,
                const char* codePrefix, DebugLineType type, int idSeed)
                {
                    ImGui::PushID(idSeed);

                    if (ImGui::CollapsingHeader(categoryName))
                    {
                        ImGui::Indent();
                        for (int i = 0; i < lines.size(); ++i)
                        {
                            auto& line = lines[i];
                            char label[64];
                            snprintf(label, 64, "%s #%d", codePrefix, i + 1);

                            ImGui::PushID(i);

                            bool isNodeOpen = ImGui::TreeNode(label);

                            if (isNodeOpen)
                            {
                                scene->m_stage->SetLineHighlight(type, i);

                                ImGui::DragFloat3("Pos", &line.Position.x, 0.1f);
                                ImGui::DragFloat3("Rot", &line.Rotation.x, 0.1f);
                                ImGui::DragFloat("Length", &line.Scale.x, 0.1f);

                                if (ImGui::Button("Copy Value"))
                                {
                                    char buffer[256];
                                    snprintf(buffer, sizeof(buffer),
                                        "// Line %s %d\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
                                        codePrefix, i + 1,
                                        line.Position.x, line.Position.y, line.Position.z,
                                        line.Rotation.x, line.Rotation.y, line.Rotation.z,
                                        line.Scale.x, line.Scale.y, line.Scale.z);
                                    ImGui::SetClipboardText(buffer);
                                }

                                ImGui::SameLine();
                                if (ImGui::Button("Delete")) {
                                    lines.erase(lines.begin() + i);
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    break;
                                }

                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }

                        if (ImGui::Button("+ Add Line"))
                        {
                            scene->m_stage->AddDebugLine(type);
                        }
                        ImGui::Unindent();
                    }
                    ImGui::PopID();
                };

            // VOID LINES (Cyan)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
            DrawLineCategory("Line Void", scene->m_stage->m_linesVoid, "Void", DebugLineType::Void, 2000);
            ImGui::PopStyleColor();

            // DISABLE LINES (Red)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            DrawLineCategory("Line Disable", scene->m_stage->m_linesDisable, "Disable", DebugLineType::Disable, 3000);
            ImGui::PopStyleColor();

            // ENABLE LINES (Green)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            DrawLineCategory("Line Enable", scene->m_stage->m_linesEnable, "Enable", DebugLineType::Enable, 4000);
            ImGui::PopStyleColor();

            // CHECKPOINT LINES (Blue)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
            DrawLineCategory("Line Checkpoint", scene->m_stage->m_linesCheckpoint, "CheckPoint", DebugLineType::Checkpoint, 5000);
            ImGui::PopStyleColor();

            ImGui::Unindent();
        }
    }

    // 3. ENEMIES TRANSFORM
    if (scene->m_enemyManager)
    {
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Debug Enemy Transform", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();
            auto& enemies = scene->m_enemyManager->GetEnemies();

            static std::unordered_map<Enemy*, int> paddleIDs;
            static int nextPaddleID = 1;
            static std::unordered_map<Enemy*, int> ballIDs;
            static int nextBallID = 1;
            static EnemySpawnConfig lastSpawnConfig;
            static bool firstRun = true;

            if (firstRun)
            {
                lastSpawnConfig.Position = { 0,0,-50 };
                lastSpawnConfig.Rotation = EnemyLevelData::Rot::Backward;
                lastSpawnConfig.Color = EnemyLevelData::Blue;

                for (int k = (int)enemies.size() - 1; k >= 0; --k)
                {
                    const auto& e = enemies[k];
                    if (e && e->GetType() == EnemyType::Paddle) {
                        lastSpawnConfig.Position = e->GetOriginalPosition();
                        lastSpawnConfig.Rotation = e->GetOriginalRotation();
                        break;
                    }
                }
                firstRun = false;
            }

            for (auto& e : enemies) e->SetHighlight(false);

            if (ImGui::TreeNode("Enemy Paddles"))
            {
                for (size_t i = 0; i < enemies.size(); ++i)
                {
                    Enemy* e = enemies[i].get();
                    if (e->GetType() != EnemyType::Paddle) continue;

                    if (paddleIDs.find(e) == paddleIDs.end()) {
                        paddleIDs[e] = nextPaddleID++;
                    }
                    int displayID = paddleIDs[e];

                    char label[64];
                    if (e->IsActive()) snprintf(label, 64, "Paddle #%d (Active)", displayID);
                    else               snprintf(label, 64, "Paddle #%d (Setup)", displayID);

                    ImGui::PushID((int)i);

                    if (ImGui::TreeNode(label))
                    {
                        e->SetHighlight(true);

                        XMFLOAT3 pos = e->GetOriginalPosition();
                        XMFLOAT3 rot = e->GetOriginalRotation();

                        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "TRANSFORM");
                        if (ImGui::DragFloat3("Pos", &pos.x, 0.1f)) {
                            e->SetPosition(pos);
                            e->UpdateOriginalTransform(pos, rot);
                        }

                        ImGui::Text("Rotation:");
                        if (ImGui::Button("Back")) { e->SetRotation(EnemyLevelData::Rot::Backward); e->UpdateOriginalTransform(pos, EnemyLevelData::Rot::Backward); } ImGui::SameLine();
                        if (ImGui::Button("Fwd")) { e->SetRotation(EnemyLevelData::Rot::Forward);  e->UpdateOriginalTransform(pos, EnemyLevelData::Rot::Forward); } ImGui::SameLine();
                        if (ImGui::Button("Left")) { e->SetRotation(EnemyLevelData::Rot::Left);     e->UpdateOriginalTransform(pos, EnemyLevelData::Rot::Left); } ImGui::SameLine();
                        if (ImGui::Button("Right")) { e->SetRotation(EnemyLevelData::Rot::Right);    e->UpdateOriginalTransform(pos, EnemyLevelData::Rot::Right); }

                        rot = e->GetRotation();
                        if (ImGui::DragFloat3("Pitch/Yaw/Roll", &rot.x, 0.1f)) {
                            e->SetRotation(rot);
                            e->UpdateOriginalTransform(pos, rot);
                        }

                        ImGui::Separator();

                        if (e->IsActive()) { ImGui::TextDisabled("Enemy is active. Copy uses original spawn coords."); }
                        else {
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "ACTIVATE BEHAVIOR");
                            if (ImGui::Button("Spawn Static", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::Static);
                                auto* newE = enemies[i].get();
                                paddleIDs[newE] = displayID;
                                lastSpawnConfig.Position = pos; lastSpawnConfig.Rotation = rot;
                            }
                            if (ImGui::Button("Spawn Tracking", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::Tracking);
                                auto* newE = enemies[i].get();
                                paddleIDs[newE] = displayID;
                                lastSpawnConfig.Position = pos; lastSpawnConfig.Rotation = rot;
                            }
                            ImGui::Spacing();
                            ImGui::Text("Horizontal:");
                            MoveDir currentMoveDir = e->GetMoveDir();
                            bool isLeft = (currentMoveDir == MoveDir::Left || currentMoveDir == MoveDir::None);
                            bool isRight = (currentMoveDir == MoveDir::Right);
                            if (ImGui::RadioButton("Left##H", isLeft)) e->SetMoveDir(MoveDir::Left);
                            ImGui::SameLine();
                            if (ImGui::RadioButton("Right##H", isRight)) e->SetMoveDir(MoveDir::Right);

                            float hMinX = e->GetMinX(); float hMaxX = e->GetMaxX();
                            if (ImGui::DragFloatRange2("Range X##H", &hMinX, &hMaxX, 0.1f)) e->SetPatrolLimitsX(hMinX, hMaxX);

                            if (ImGui::Button("Spawn Tracking Horizontal", ImVec2(-1, 0))) {
                                MoveDir dir = (currentMoveDir == MoveDir::None) ? MoveDir::Left : currentMoveDir;
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::TrackingHorizontal, dir, e->GetMinX(), e->GetMaxX());
                                auto* newE = enemies[i].get();
                                paddleIDs[newE] = displayID;
                                lastSpawnConfig.Position = pos; lastSpawnConfig.Rotation = rot;
                            }

                            ImGui::Spacing();
                            float rMinX = e->GetMinX(); float rMaxX = e->GetMaxX();
                            float rMinZ = e->GetMinZ(); float rMaxZ = e->GetMaxZ();
                            ImGui::Text("Random:");
                            if (ImGui::DragFloatRange2("Rand Range X", &rMinX, &rMaxX, 0.1f)) e->SetPatrolLimitsX(rMinX, rMaxX);
                            if (ImGui::DragFloatRange2("Rand Range Z", &rMinZ, &rMaxZ, 0.1f)) e->SetPatrolLimitsZ(rMinZ, rMaxZ);

                            if (ImGui::Button("Spawn Tracking Random", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::TrackingRandom, MoveDir::None, e->GetMinX(), e->GetMaxX(), e->GetMinZ(), e->GetMaxZ());
                                auto* newE = enemies[i].get();
                                paddleIDs[newE] = displayID;
                                lastSpawnConfig.Position = pos; lastSpawnConfig.Rotation = rot;
                            }
                        }

                        ImGui::Spacing();
                        if (ImGui::Button("Copy Value")) {
                            std::string copyStr = GenerateEnemyCopyString(e, displayID, "Paddle", pos);
                            ImGui::SetClipboardText(copyStr.c_str());
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Delete")) {
                            enemies.erase(enemies.begin() + i);
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Spacing();
                if (ImGui::Button("+ Add New Paddle", ImVec2(-1, 30)))
                {
                    EnemySpawnConfig cfg = lastSpawnConfig;
                    cfg.Type = EnemyType::Paddle;
                    cfg.Color = EnemyLevelData::Blue;
                    scene->m_enemyManager->SpawnEnemy(cfg);
                    auto& newE = scene->m_enemyManager->GetEnemies().back();
                    newE->SetActive(false);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Enemy Balls"))
            {
                for (size_t i = 0; i < enemies.size(); ++i)
                {
                    Enemy* e = enemies[i].get();
                    if (e->GetType() != EnemyType::Ball) continue;

                    if (ballIDs.find(e) == ballIDs.end()) {
                        ballIDs[e] = nextBallID++;
                    }
                    int displayID = ballIDs[e];
                    char label[64];
                    snprintf(label, 64, "Ball #%d", displayID);

                    ImGui::PushID((int)i);
                    if (ImGui::TreeNode(label))
                    {
                        e->SetHighlight(true);

                        XMFLOAT3 pos = e->GetOriginalPosition();
                        XMFLOAT3 rot = e->GetOriginalRotation();
                        XMFLOAT3 scl = e->scale;

                        ImGui::DragFloat3("Pos", &pos.x, 0.1f);
                        ImGui::DragFloat3("Rot", &rot.x, 0.1f);
                        ImGui::DragFloat3("Scale", &scl.x, 0.1f);

                        e->SetPosition(pos);
                        e->SetRotation(rot);
                        e->scale = scl;

                        e->UpdateOriginalTransform(pos, rot);

                        if (ImGui::Button("Copy Value")) {
                            std::string copyStr = GenerateEnemyCopyString(e, displayID, "Ball", pos);
                            ImGui::SetClipboardText(copyStr.c_str());
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Delete")) {
                            enemies.erase(enemies.begin() + i);
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Spacing();
                if (ImGui::Button("+ Add New Ball", ImVec2(-1, 30)))
                {
                    EnemySpawnConfig cfg;

                    bool foundLastBall = false;
                    for (int k = (int)enemies.size() - 1; k >= 0; --k)
                    {
                        if (enemies[k]->GetType() == EnemyType::Ball) {
                            cfg.Position = enemies[k]->GetPosition();
                            cfg.Rotation = enemies[k]->GetRotation();
                            foundLastBall = true;
                            break;
                        }
                    }

                    if (!foundLastBall)
                    {
                        cfg.Position = { 0,0,0 };
                        cfg.Rotation = { 0,0,0 };
                    }

                    cfg.Color = EnemyLevelData::PaleYellow;
                    cfg.Type = EnemyType::Ball;
                    scene->m_enemyManager->SpawnEnemy(cfg);
                }

                ImGui::TreePop();
            }

            ImGui::Unindent();
        }
    }

    if (scene->m_itemManager)
    {
        if (ImGui::CollapsingHeader("Item Transform", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();
            scene->m_itemManager->SetHighlight(-1);
            auto& items = scene->m_itemManager->GetItems();
            static std::unordered_map<Item*, int> healIDs;
            static int nextHealID = 1;
            static std::unordered_map<Item*, int> invIDs;
            static int nextInvID = 1;

            auto DrawSingleItemNode = [&](int index, Item* item, int displayIndex, const char* namePrefix) -> bool
                {
                    char label[64];
                    snprintf(label, 64, "%s #%d", namePrefix, displayIndex);

                    ImGui::PushID(index);
                    bool deleted = false;

                    if (ImGui::TreeNode(label))
                    {
                        scene->m_itemManager->SetHighlight(index);

                        const char* typeNames[] = { "Heal", "Invincible" };
                        int currentTypeIdx = (item->GetType() == ItemType::Heal) ? 0 : 1;

                        if (ImGui::Combo("Change Type", &currentTypeIdx, typeNames, IM_ARRAYSIZE(typeNames)))
                        {
                            item->SetType((currentTypeIdx == 0) ? ItemType::Heal : ItemType::Invincible);
                        }

                        XMFLOAT3 pos = item->GetBasePosition();
                        XMFLOAT3 rot = item->GetRotation();
                        XMFLOAT3 scl = item->scale;

                        ImGui::DragFloat3("Pos", &pos.x, 0.1f);
                        ImGui::DragFloat3("Rot", &rot.x, 0.1f);
                        ImGui::DragFloat3("Scale", &scl.x, 0.1f);

                        item->SetPosition(pos);
                        item->SetRotation(rot);
                        item->scale = scl;

                        if (ImGui::Button("Copy Value"))
                        {
                            char buffer[512];
                            const char* typeEnum = (item->GetType() == ItemType::Heal) ? "ItemType::Heal" : "ItemType::Invincible";

                            snprintf(buffer, sizeof(buffer),
                                "// %s\n{ { %.1ff, %.1ff, %.1ff }, { 0.0f, 0.0f, 0.0f }, { %.1ff, %.1ff, %.1ff }, %s },",
                                label,
                                pos.x, pos.y, pos.z,
                                scl.x, scl.y, scl.z,
                                typeEnum
                            );
                            ImGui::SetClipboardText(buffer);
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Delete"))
                        {
                            items.erase(items.begin() + index);
                            deleted = true;
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    return deleted;
                };

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "--- HEAL ITEMS ---");
            for (int i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Heal)
                {
                    Item* ptr = items[i].get();
                    if (healIDs.find(ptr) == healIDs.end()) {
                        healIDs[ptr] = nextHealID++;
                    }
                    int displayID = healIDs[ptr];

                    if (DrawSingleItemNode(i, items[i].get(), displayID, "Item Heal")) break;
                }
            }

            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "--- INVINCIBLE ITEMS ---");
            for (int i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Invincible)
                {
                    Item* ptr = items[i].get();
                    if (invIDs.find(ptr) == invIDs.end()) {
                        invIDs[ptr] = nextInvID++;
                    }
                    int displayID = invIDs[ptr];

                    if (DrawSingleItemNode(i, items[i].get(), displayID, "Item Invincible")) break;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            static int spawnItemType = 0;
            const char* itemTypeNames[] = { "Heal", "Invincible" };

            ImGui::Text("Spawn Controls:");
            ImGui::Combo("New Item Type", &spawnItemType, itemTypeNames, IM_ARRAYSIZE(itemTypeNames));

            if (ImGui::Button("+ Spawn Item"))
            {
                ItemType type = (spawnItemType == 0) ? ItemType::Heal : ItemType::Invincible;
                scene->m_itemManager->AddItem(type);
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset Animation"))
            {
                scene->m_itemManager->ResetAllAnimations();
            }
            ImGui::Unindent();
        }
    }
}