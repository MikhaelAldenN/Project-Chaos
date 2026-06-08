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
#include <string_view>
#include <cstdio>
#include <unordered_map>

using namespace DirectX;

// ==========================================
// MODERN C++ HELPERS (Zero Runtime Cost)
// ==========================================

constexpr std::string_view GetEnemyTypeName(EnemyType type) noexcept
{
    switch (type)
    {
    case EnemyType::Paddle:             return "Paddle";
    case EnemyType::Ball:               return "Ball";
    case EnemyType::MushroomNone:       return "Mushroom None";
    case EnemyType::MushroomStatic:     return "Mushroom Static";
    case EnemyType::MushroomTracking:   return "Mushroom Tracking";
    case EnemyType::FakeBoss:           return "Fake Boss";
    default:                            return "Unknown";
    }
}

constexpr std::string_view GetEnemyTypeCode(EnemyType type) noexcept
{
    switch (type)
    {
    case EnemyType::Paddle:             return "EnemyType::Paddle";
    case EnemyType::Ball:               return "EnemyType::Ball";
    case EnemyType::MushroomNone:       return "EnemyType::MushroomNone";
    case EnemyType::MushroomStatic:     return "EnemyType::MushroomStatic";
    case EnemyType::MushroomTracking:   return "EnemyType::MushroomTracking";
    case EnemyType::FakeBoss:           return "EnemyType::FakeBoss";
    default:                            return "EnemyType::Paddle";
    }
}

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

    XMFLOAT3 pos{ currentPos };
    XMFLOAT3 rot{ e->GetOriginalRotation() };

    std::string rotStr{ "{ " + FloatToString(rot.x) + ", " + FloatToString(rot.y) + ", " + FloatToString(rot.z) + " }" };
    if (abs(rot.y - 0.0f) < 0.01f) rotStr = "Rot::Backward";
    else if (abs(rot.y - DirectX::XM_PI) < 0.01f) rotStr = "Rot::Forward";
    else if (abs(rot.y - DirectX::XM_PIDIV2) < 0.01f) rotStr = "Rot::Left";
    else if (abs(rot.y + DirectX::XM_PIDIV2) < 0.01f) rotStr = "Rot::Right";

    std::string typeStr{ GetEnemyTypeCode(e->GetType()) };

    // Explicit color parsing for universal support
    XMFLOAT4 c{ e->GetBaseColor() };
    char colorBuf[64];
    snprintf(colorBuf, sizeof(colorBuf), "{ %.2ff, %.2ff, %.2ff, %.2ff }", c.x, c.y, c.z, c.w);
    std::string colorStr{ colorBuf };

    std::string attackStr{ "AttackType::None" };
    std::string extraParams{ "" };

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

    std::string posStr{ "{ " + FloatToString(pos.x) + ", " + FloatToString(pos.y) + ", " + FloatToString(pos.z) + " }" };

    snprintf(buffer, sizeof(buffer),
        "// %s %d\n{ %s, %s, %s, %s, %s%s },",
        commentName, index, posStr.c_str(), rotStr.c_str(), colorStr.c_str(), typeStr.c_str(), attackStr.c_str(), extraParams.c_str());

    return std::string(buffer);
}

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
    if (ImGui::DragFloat3("##LightDir", &light.direction.x, 0.01f, -1.0f, 1.0f)) lightChanged = true;

    ImGui::Text("Light Color");
    if (ImGui::ColorEdit3("##LightColor", &light.color.x)) lightChanged = true;

    if (lightChanged)
    {
        XMVECTOR vDir = XMLoadFloat3(&light.direction);
        vDir = XMVector3Normalize(vDir);
        XMStoreFloat3(&light.direction, vDir);
        scene->m_lightManager.SetDirectionalLight(light);
    }
}

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

        ImGui::SliderFloat("Intensity", &scene->m_uberParams.chromaticAberration, -0.02f, 0.02f, "%.5f");
        if (ImGui::Button("Reset CA")) scene->m_uberParams.chromaticAberration = 0.0f;

        if (!scene->m_fxState.EnableChromatic) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("HDR Bloom", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: Bloom", &scene->m_fxState.EnableBloom);
        if (!scene->m_fxState.EnableBloom) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::DragFloat("Bloom Threshold", &uber.bloomThreshold, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Bloom Intensity", &uber.bloomIntensity, 0.05f, 0.0f, 10.0f);

        if (ImGui::Button("Reset Bloom", ImVec2(-1, 0)))
        {
            uber.bloomThreshold = 1.0f;
            uber.bloomIntensity = 1.5f;
        }

        if (!scene->m_fxState.EnableBloom) ImGui::PopStyleVar();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("PSX Retro Filter", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Checkbox("ACTIVATE: PSX Filter", &scene->m_fxState.EnablePSX);
        if (!scene->m_fxState.EnablePSX) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

        ImGui::SliderFloat("Resolution Width", &uber.psxResWidth, 160.0f, 1920.0f);
        ImGui::SliderFloat("Resolution Height", &uber.psxResHeight, 120.0f, 1080.0f);
        ImGui::SliderFloat("Color Depth (Banding)", &uber.psxColorDepth, 2.0f, 255.0f);
        ImGui::SliderFloat("Dither Strength", &uber.psxDitherStrength, 0.0f, 2.0f);

        if (!scene->m_fxState.EnablePSX) ImGui::PopStyleVar();
        ImGui::Unindent();
    }
}

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

            auto& enemies{ scene->m_enemyManager->GetEnemies() };

            auto DrawEnemyColor = [](Enemy* e, int index, const char* label)
                {
                    ImGui::PushID(index);
                    ImGui::ColorEdit4(label, &e->GetMutableBaseColor().x);
                    ImGui::PopID();
                };

            // Modern Map tracking guarantees safety, avoids dangling map pointers from erased enemies
            std::unordered_map<EnemyType, int> typeCounters;

            for (size_t i = 0; i < enemies.size(); ++i)
            {
                Enemy* e{ enemies[i].get() };

                // Exclude the pentagon as requested, prevent null pointers
                if (!e || e->GetType() == EnemyType::Pentagon) continue;

                const EnemyType t{ e->GetType() };
                const int count{ ++typeCounters[t] };

                char label[128];
                snprintf(label, sizeof(label), "%s #%d", GetEnemyTypeName(t).data(), count);
                DrawEnemyColor(e, static_cast<int>(i), label);
            }
            ImGui::Unindent();
        }
        ImGui::Spacing();
    }

    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "ITEMS");
    ImGui::Separator();
    ImGui::Spacing();

    if (scene->m_itemManager)
    {
        if (ImGui::CollapsingHeader("Items"))
        {
            ImGui::Indent();
            auto& items = scene->m_itemManager->GetItems();
            int healCount = 0;

            for (size_t i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Heal)
                {
                    healCount++;
                    char label[64];
                    snprintf(label, 64, "Item Heal #%d", healCount);

                    ImGui::PushID(static_cast<int>(i));
                    ImGui::ColorEdit4(label, &items[i]->color.x);
                    ImGui::PopID();
                }
            }
            ImGui::Unindent();
        }
    }

    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "BULLET");
    ImGui::Separator();
    ImGui::Spacing();

    if (scene->m_player)
    {
        if (ImGui::CollapsingHeader("Player Bullet Color", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##PBColor", &scene->m_player->GetPlayerBulletColor()->x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            ImGui::Unindent();
        }
        ImGui::Spacing();
    }
}

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

            for (size_t i = 0; i < scene->m_stage->m_debugWalls.size(); ++i)
            {
                auto& wall = scene->m_stage->m_debugWalls[i];
                char label[32];
                snprintf(label, 32, "Wall #%zu", i + 1);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode(label))
                {
                    scene->m_stage->SetWallHighlight(static_cast<int>(i));

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
                            "// Wall %zu\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
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
                        for (size_t i = 0; i < lines.size(); ++i)
                        {
                            auto& line = lines[i];
                            char label[64];
                            snprintf(label, 64, "%s #%zu", codePrefix, i + 1);

                            ImGui::PushID(static_cast<int>(i));

                            bool isNodeOpen = ImGui::TreeNode(label);

                            if (isNodeOpen)
                            {
                                scene->m_stage->SetLineHighlight(type, static_cast<int>(i));

                                ImGui::DragFloat3("Pos", &line.Position.x, 0.1f);
                                ImGui::DragFloat3("Rot", &line.Rotation.x, 0.1f);
                                ImGui::DragFloat("Length", &line.Scale.x, 0.1f);

                                if (ImGui::Button("Copy Value"))
                                {
                                    char buffer[256];
                                    snprintf(buffer, sizeof(buffer),
                                        "// Line %s %zu\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
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

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
            DrawLineCategory("Line Void", scene->m_stage->m_linesVoid, "Void", DebugLineType::Void, 2000);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            DrawLineCategory("Line Disable", scene->m_stage->m_linesDisable, "Disable", DebugLineType::Disable, 3000);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            DrawLineCategory("Line Enable", scene->m_stage->m_linesEnable, "Enable", DebugLineType::Enable, 4000);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
            DrawLineCategory("Line Checkpoint", scene->m_stage->m_linesCheckpoint, "CheckPoint", DebugLineType::Checkpoint, 5000);
            ImGui::PopStyleColor();

            ImGui::Unindent();
        }
    }

    // 3. ENEMIES TRANSFORM (REFACTORED - DRY & Memory Safe)
    if (scene->m_enemyManager)
    {
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Debug Enemy Transform", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();
            auto& enemies{ scene->m_enemyManager->GetEnemies() };

            // Turn off all highlights so they only highlight while actively opened in tree
            for (auto& e : enemies) e->SetHighlight(false);

            // Using constexpr guarantees zero runtime allocation for iterating supported types
            constexpr EnemyType editableTypes[] = {
                EnemyType::Paddle, EnemyType::Ball, EnemyType::MushroomNone,
                EnemyType::MushroomStatic, EnemyType::MushroomTracking, EnemyType::FakeBoss
            };

            for (const EnemyType currentType : editableTypes)
            {
                std::string headerName{ std::string(GetEnemyTypeName(currentType)) + "s" };

                if (ImGui::TreeNode(headerName.c_str()))
                {
                    int displayID{ 1 };

                    for (size_t i = 0; i < enemies.size(); ++i)
                    {
                        Enemy* e{ enemies[i].get() };
                        if (!e || e->GetType() != currentType) continue;

                        char label[64];
                        snprintf(label, sizeof(label), "%s #%d%s",
                            GetEnemyTypeName(currentType).data(),
                            displayID,
                            e->IsActive() ? " (Active)" : " (Setup)");

                        ImGui::PushID(static_cast<int>(i));

                        if (ImGui::TreeNode(label))
                        {
                            e->SetHighlight(true);

                            XMFLOAT3 pos{ e->GetOriginalPosition() };
                            XMFLOAT3 rot{ e->GetOriginalRotation() };

                            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "TRANSFORM");
                            if (ImGui::DragFloat3("Pos", &pos.x, 0.1f)) {
                                e->SetPosition(pos);
                                e->UpdateOriginalTransform(pos, rot);
                            }

                            XMFLOAT3 scl{ e->GetScale() };
                            if (ImGui::DragFloat3("Scale", &scl.x, 0.1f)) {
                                e->SetScale(scl);
                            }

                            ImGui::Text("Rotation Preset:");
                            if (ImGui::Button("Back")) { rot = EnemyLevelData::Rot::Backward; e->SetRotation(rot); e->UpdateOriginalTransform(pos, rot); } ImGui::SameLine();
                            if (ImGui::Button("Fwd")) { rot = EnemyLevelData::Rot::Forward;  e->SetRotation(rot); e->UpdateOriginalTransform(pos, rot); } ImGui::SameLine();
                            if (ImGui::Button("Left")) { rot = EnemyLevelData::Rot::Left;     e->SetRotation(rot); e->UpdateOriginalTransform(pos, rot); } ImGui::SameLine();
                            if (ImGui::Button("Right")) { rot = EnemyLevelData::Rot::Right;    e->SetRotation(rot); e->UpdateOriginalTransform(pos, rot); }

                            if (ImGui::DragFloat3("Pitch/Yaw/Roll", &rot.x, 0.1f)) {
                                e->SetRotation(rot);
                                e->UpdateOriginalTransform(pos, rot);
                            }

                            ImGui::Separator();

                            // ----------------------------------------------------
                            // BUG FIX: Allow behavior editing ANY time, even when active
                            // ----------------------------------------------------
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "ACTIVATE BEHAVIOR");

                            if (ImGui::Button("Spawn Static", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::Static);
                            }
                            if (ImGui::Button("Spawn Tracking", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::Tracking);
                            }

                            ImGui::Spacing();
                            ImGui::Text("Horizontal:");
                            const MoveDir currentMoveDir{ e->GetMoveDir() };
                            const bool isLeft{ currentMoveDir == MoveDir::Left || currentMoveDir == MoveDir::None };
                            const bool isRight{ currentMoveDir == MoveDir::Right };

                            if (ImGui::RadioButton("Left##H", isLeft)) e->SetMoveDir(MoveDir::Left);
                            ImGui::SameLine();
                            if (ImGui::RadioButton("Right##H", isRight)) e->SetMoveDir(MoveDir::Right);

                            float hMinX{ e->GetMinX() }, hMaxX{ e->GetMaxX() };
                            if (ImGui::DragFloatRange2("Range X##H", &hMinX, &hMaxX, 0.1f)) e->SetPatrolLimitsX(hMinX, hMaxX);

                            if (ImGui::Button("Spawn Tracking Horizontal", ImVec2(-1, 0))) {
                                const MoveDir dir{ currentMoveDir == MoveDir::None ? MoveDir::Left : currentMoveDir };
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::TrackingHorizontal, dir, e->GetMinX(), e->GetMaxX());
                            }

                            ImGui::Spacing();
                            float rMinX{ e->GetMinX() }, rMaxX{ e->GetMaxX() };
                            float rMinZ{ e->GetMinZ() }, rMaxZ{ e->GetMaxZ() };
                            ImGui::Text("Random:");
                            if (ImGui::DragFloatRange2("Rand Range X", &rMinX, &rMaxX, 0.1f)) e->SetPatrolLimitsX(rMinX, rMaxX);
                            if (ImGui::DragFloatRange2("Rand Range Z", &rMinZ, &rMaxZ, 0.1f)) e->SetPatrolLimitsZ(rMinZ, rMaxZ);

                            if (ImGui::Button("Spawn Tracking Random", ImVec2(-1, 0))) {
                                scene->m_enemyManager->RespawnEnemyAs(i, AttackType::TrackingRandom, MoveDir::None, e->GetMinX(), e->GetMaxX(), e->GetMinZ(), e->GetMaxZ());
                            }

                            ImGui::Spacing();
                            if (ImGui::Button("Copy Value")) {
                                std::string copyStr{ GenerateEnemyCopyString(e, displayID, GetEnemyTypeName(currentType).data(), pos) };
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
                        ++displayID;
                    }

                    ImGui::Spacing();
                    std::string addBtnText{ "+ Add New " + std::string(GetEnemyTypeName(currentType)) };
                    if (ImGui::Button(addBtnText.c_str(), ImVec2(-1, 30)))
                    {
                        EnemySpawnConfig cfg;
                        bool foundRef{ false };

                        // Grab position of the last spawned enemy of this specific type for user convenience
                        for (int k = static_cast<int>(enemies.size()) - 1; k >= 0; --k)
                        {
                            if (enemies[k] && enemies[k]->GetType() == currentType) {
                                cfg.Position = enemies[k]->GetOriginalPosition();
                                cfg.Rotation = enemies[k]->GetOriginalRotation();
                                cfg.Scale = enemies[k]->GetScale();
                                cfg.Color = enemies[k]->GetBaseColor();
                                foundRef = true;
                                break;
                            }
                        }

                        if (!foundRef) {
                            cfg.Position = { 0.0f, 0.0f, 0.0f };
                            cfg.Rotation = EnemyLevelData::Rot::Backward;
                            cfg.Scale = { 1.0f, 1.0f, 1.0f };
                            cfg.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                        }

                        cfg.Type = currentType;
                        scene->m_enemyManager->SpawnEnemy(cfg);
                    }
                    ImGui::TreePop();
                }
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
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Heal)
                {
                    Item* ptr = items[i].get();
                    if (healIDs.find(ptr) == healIDs.end()) {
                        healIDs[ptr] = nextHealID++;
                    }
                    int displayID = healIDs[ptr];

                    if (DrawSingleItemNode(static_cast<int>(i), items[i].get(), displayID, "Item Heal")) break;
                }
            }

            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "--- INVINCIBLE ITEMS ---");
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (items[i]->GetType() == ItemType::Invincible)
                {
                    Item* ptr = items[i].get();
                    if (invIDs.find(ptr) == invIDs.end()) {
                        invIDs[ptr] = nextInvID++;
                    }
                    int displayID = invIDs[ptr];

                    if (DrawSingleItemNode(static_cast<int>(i), items[i].get(), displayID, "Item Invincible")) break;
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

    // 4. WEAPON TRANSFORM
    ImGui::Spacing();
    if (scene->m_player)
    {
        if (ImGui::CollapsingHeader("Weapon Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            static int selectedWeaponIdx = 0;
            const char* weaponNames[] = { "Crossbow", "Sword" };
            ImGui::Combo("Select Weapon", &selectedWeaponIdx, weaponNames, IM_ARRAYSIZE(weaponNames));

            Player::WeaponType selectedType = static_cast<Player::WeaponType>(selectedWeaponIdx);
            Weapon* weapon = scene->m_player->GetWeapon(selectedType);

            if (weapon)
            {
                bool isActive = (scene->m_player->GetActiveWeaponType() == selectedType);
                if (ImGui::Checkbox("Force Equip (Debug View)", &isActive))
                {
                    scene->m_player->SetActiveWeapon(isActive ? selectedType : Player::WeaponType::Crossbow);

                    if (!isActive) {
                        scene->m_player->GetDebugState().forceAnimation = false;
                        scene->m_player->GetDebugState().disableAimConstraint = false;
                    }
                }

                if (isActive)
                {
                    ImGui::Indent();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "ANIMATION DEBUGGER");

                    auto& debugState = scene->m_player->GetDebugState();
                    ImGui::Checkbox("Disable Aim Twist (Straight Spine)", &debugState.disableAimConstraint);

                    static int debugAnimIdx = 0;
                    const char* animLabels[] = { "Normal State Machine", "Loop 'Slash'", "Loop 'Parry'" };
                    const char* actualAnimNames[] = { "", "Slash", "Parry" };

                    if (ImGui::Combo("Animation Mode", &debugAnimIdx, animLabels, IM_ARRAYSIZE(animLabels)))
                    {
                        if (debugAnimIdx == 0) {
                            debugState.forceAnimation = false;
                        }
                        else {
                            debugState.forceAnimation = true;
                            debugState.animationName = actualAnimNames[debugAnimIdx];
                        }
                    }
                    ImGui::Unindent();
                }
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET POSITION");
                ImGui::DragFloat3("XYZ##WpnPos", &weapon->GetOffsetPos()->x, 0.01f);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET ROTATION");
                ImGui::DragFloat3("Pitch/Yaw/Roll##WpnRot", &weapon->GetOffsetRot()->x, 1.0f, -180.0f, 180.0f);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SCALE MULTIPLIER");
                ImGui::DragFloat3("XYZ##WpnScl", &weapon->GetOffsetScale()->x, 0.01f, 0.1f, 10.0f);

                ImGui::Spacing();
                ImGui::Separator();

                if (ImGui::Button("Reset Weapon Transform", ImVec2(-1, 30)))
                {
                    weapon->ResetOffsets();
                }

                ImGui::Spacing();
                if (ImGui::Button("Copy Code to Clipboard", ImVec2(-1, 30)))
                {
                    char buffer[512];
                    const char* enumName = (selectedType == Player::WeaponType::Sword) ? "WeaponType::Sword" : "WeaponType::Crossbow";

                    snprintf(buffer, sizeof(buffer),
                        "m_weapons[static_cast<size_t>(%s)]->SetLocalOffset(\n"
                        "{ %.3ff, %.3ff, %.3ff },\n"
                        "{ %.3ff, %.3ff, %.3ff },\n"
                        "{ %.3ff, %.3ff, %.3ff }\n"
                        ");",
                        enumName,
                        weapon->GetOffsetPos()->x, weapon->GetOffsetPos()->y, weapon->GetOffsetPos()->z,
                        weapon->GetOffsetRot()->x, weapon->GetOffsetRot()->y, weapon->GetOffsetRot()->z,
                        weapon->GetOffsetScale()->x, weapon->GetOffsetScale()->y, weapon->GetOffsetScale()->z
                    );
                    ImGui::SetClipboardText(buffer);
                }
            }
            ImGui::Unindent();
        }
    }

    // 5. PLAYER BULLET TRANSFORM
    ImGui::Spacing();
    if (scene->m_player && scene->m_player->GetPlayerBulletModel())
    {
        if (ImGui::CollapsingHeader("Player Bullet Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            Player* p = scene->m_player.get();

            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET POSITION");
            ImGui::DragFloat3("XYZ##PBPos", &p->GetPlayerBulletOffsetPos()->x, 0.01f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET ROTATION");
            ImGui::DragFloat3("Pitch/Yaw/Roll##PBRot", &p->GetPlayerBulletOffsetRot()->x, 1.0f, -180.0f, 180.0f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SCALE MULTIPLIER");
            ImGui::DragFloat3("XYZ##PBScl", &p->GetPlayerBulletOffsetScale()->x, 0.01f, 0.1f, 10.0f);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Reset Player Bullet Transform", ImVec2(-1, 30)))
            {
                p->ResetPlayerBulletOffsets();
            }

            ImGui::Spacing();
            if (ImGui::Button("Copy Bullet Code to Clipboard", ImVec2(-1, 30)))
            {
                char buffer[512];
                snprintf(buffer, sizeof(buffer),
                    "DirectX::XMFLOAT3 m_playerbulletOffsetPos   { %.3ff, %.3ff, %.3ff };\n"
                    "DirectX::XMFLOAT3 m_playerbulletOffsetRot   { %.3ff, %.3ff, %.3ff };\n"
                    "DirectX::XMFLOAT3 m_playerbulletOffsetScale { %.3ff, %.3ff, %.3ff };\n"
                    "DirectX::XMFLOAT4 m_playerbulletColor       { %.3ff, %.3ff, %.3ff, %.3ff };",
                    p->GetPlayerBulletOffsetPos()->x, p->GetPlayerBulletOffsetPos()->y, p->GetPlayerBulletOffsetPos()->z,
                    p->GetPlayerBulletOffsetRot()->x, p->GetPlayerBulletOffsetRot()->y, p->GetPlayerBulletOffsetRot()->z,
                    p->GetPlayerBulletOffsetScale()->x, p->GetPlayerBulletOffsetScale()->y, p->GetPlayerBulletOffsetScale()->z,
                    p->GetPlayerBulletColor()->x, p->GetPlayerBulletColor()->y, p->GetPlayerBulletColor()->z, p->GetPlayerBulletColor()->w
                );
                ImGui::SetClipboardText(buffer);
            }
            ImGui::Unindent();
        }
    }

    // 6. CAPE PHYSICS TUNING
    ImGui::Spacing();
    if (scene->m_player && scene->m_player->GetCapeSimulator())
    {
        if (ImGui::CollapsingHeader("Cape Physics Tuning", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            CapeSimulator* cape = scene->m_player->GetCapeSimulator();

            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SPRING DYNAMICS (FEEL)");
            ImGui::DragFloat("Stiffness", cape->GetStiffness(), 1.0f, 10.0f, 500.0f, "%.1f");
            ImGui::DragFloat("Damping", cape->GetDamping(), 0.1f, 1.0f, 50.0f, "%.1f");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MOVEMENT & GRAVITY");

            ImGui::DragFloat("Body Clip Limit", cape->GetBodyClipLimit(), 0.05f, -3.14f, 3.14f, "%.2f");
            ImGui::DragFloat("Max Sway (Radians)", cape->GetMaxSway(), 0.05f, 0.1f, 3.14f, "%.2f");
            ImGui::DragFloat("Gravity Angle X", cape->GetGravityAngle(), 0.05f, -1.0f, 2.0f, "%.2f");
            ImGui::DragFloat("Velocity Sensitivity", cape->GetSwaySensitivity(), 0.005f, 0.0f, 0.2f, "%.3f");

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Reset Default Cape Physics", ImVec2(-1, 30)))
            {
                *cape->GetStiffness() = 150.0f;
                *cape->GetDamping() = 12.0f;
                *cape->GetMaxSway() = 1.2f;
                *cape->GetGravityAngle() = 0.4f;
                *cape->GetSwaySensitivity() = 0.03f;
            }

            ImGui::Spacing();
            if (ImGui::Button("Copy Cape Config to Clipboard", ImVec2(-1, 30)))
            {
                char buffer[512];
                snprintf(buffer, sizeof(buffer),
                    "float m_stiffness{ %.1ff };\n"
                    "float m_damping{ %.1ff };\n"
                    "float m_maxSway{ %.2ff };\n"
                    "float m_bodyClipLimit{ %.2ff };\n"
                    "float m_gravityAngleX{ %.2ff };\n"
                    "float m_swaySensitivity{ %.3ff };",
                    *cape->GetStiffness(),
                    *cape->GetDamping(),
                    *cape->GetMaxSway(),
                    *cape->GetGravityAngle(),
                    *cape->GetSwaySensitivity()
                );
                ImGui::SetClipboardText(buffer);
            }

            ImGui::Unindent();
        }
    }

    // 7. BOSS CINEMATIC EFFECT TUNING
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Boss Effect Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET POSITION");
        ImGui::DragFloat3("XYZ##BossEffOff", &scene->m_fakeBossEffectOffset.x, 0.01f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LOCAL OFFSET ROTATION");
        ImGui::DragFloat3("Pitch/Yaw/Roll##BossEffRot", &scene->m_fakeBossEffectRotation.x, 1.0f, -180.0f, 180.0f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SCALE MULTIPLIER");
        ImGui::DragFloat("Scale##BossEffScl", &scene->m_fakeBossEffectScale, 0.05f, 0.1f, 100.0f);

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Play Test Effect Now (Preview)", ImVec2(-1, 30)))
        {
            if (Enemy* fakeBoss = scene->GetFakeBoss())
            {
                DirectX::XMFLOAT3 testPos = fakeBoss->GetPosition();
                testPos.x += scene->m_fakeBossEffectOffset.x;
                testPos.y += scene->m_fakeBossEffectOffset.y;
                testPos.z += scene->m_fakeBossEffectOffset.z;

                Effekseer::Handle effHandle = EffectManager::Instance().Play(
                    "Data/Effect/FakeBossPoison.efk",
                    testPos,
                    scene->m_fakeBossEffectScale
                );

                if (effHandle >= 0)
                {
                    DirectX::XMFLOAT3 rotRad{
                        DirectX::XMConvertToRadians(scene->m_fakeBossEffectRotation.x),
                        DirectX::XMConvertToRadians(scene->m_fakeBossEffectRotation.y),
                        DirectX::XMConvertToRadians(scene->m_fakeBossEffectRotation.z)
                    };
                    EffectManager::Instance().SetRotation(effHandle, rotRad);
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Copy Boss Effect Code", ImVec2(-1, 30)))
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                "float m_fakeBossEffectScale{ %.3ff };\n"
                "DirectX::XMFLOAT3 m_fakeBossEffectOffset{ %.3ff, %.3ff, %.3ff };\n"
                "DirectX::XMFLOAT3 m_fakeBossEffectRotation{ %.3ff, %.3ff, %.3ff };",
                scene->m_fakeBossEffectScale,
                scene->m_fakeBossEffectOffset.x, scene->m_fakeBossEffectOffset.y, scene->m_fakeBossEffectOffset.z,
                scene->m_fakeBossEffectRotation.x, scene->m_fakeBossEffectRotation.y, scene->m_fakeBossEffectRotation.z
            );
            ImGui::SetClipboardText(buffer);
        }

        ImGui::Unindent();
    }
}