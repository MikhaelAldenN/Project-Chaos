#include "Enemy.h"
#include "CameraController.h"
#include "CinematicDirector.h"
#include "GUISceneGameBreaker.h"
#include "SceneGameBreaker.h" 
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdio> 

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

void GameBreakerGUI::Draw(SceneGameBreaker* scene)
{
    // 1. Debug Controller (Window Terpisah milik CameraController)
    CameraController::Instance().DrawDebugGUI();

    // 2. Window Utama "Scene Inspector"
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Inspector", nullptr))
    {
        if (ImGui::BeginTabBar("InspectorTabs"))
        {
            // --- TAB 1: CAMERA ---
            if (ImGui::BeginTabItem("Camera Info"))
            {
                DrawCameraTab(scene);
                ImGui::EndTabItem();
            }

            // --- TAB 2: UI LAYOUT ---
            if (ImGui::BeginTabItem("UI Layout"))
            {
                DrawUI_LayoutTab(scene);
                ImGui::EndTabItem();
            }

            // --- TAB 3: POST PROCESS ---
            if (ImGui::BeginTabItem("Post-Process & FX"))
            {
                DrawPostProcessTab(scene);
                ImGui::EndTabItem();
            }

            // --- TAB 4: OBJECT COLOR ---
            if (ImGui::BeginTabItem("Object Color"))
            {
                DrawObjectColorTab(scene);
                ImGui::EndTabItem();
            }

            // --- TAB 5: OBJECT TRANSFORM ---
            if (ImGui::BeginTabItem("Object Transform"))
            {
                DrawObjectTransformTab(scene);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // 3. Window Sprite (Window Terpisah)
    DrawSpriteTab(scene);
}

void GameBreakerGUI::DrawCameraTab(SceneGameBreaker* scene)
{
    auto& camCtrl = CameraController::Instance();
    auto mainCam = scene->GetMainCamera();

    // [PERBAIKAN] Akses data dari m_director, bukan langsung dari scene
    // Pastikan m_director ada (safety check)
    if (!scene->m_director)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: CinematicDirector is NULL!");
        return;
    }

    // Ambil referensi vector dari Director
    auto& scenarios = scene->m_director->GetScenarios();

    static bool isSequencePlaying = false;

    // Sync state dengan controller
    bool ctrlIsSequencing = camCtrl.IsSequencing();
    if (!ctrlIsSequencing) isSequencePlaying = false;

    // --- BUTTONS ---
    float availWidth = ImGui::GetContentRegionAvail().x;
    if (isSequencePlaying)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP SEQUENCE", ImVec2(availWidth, 40)))
        {
            camCtrl.StopSequence();
            isSequencePlaying = false;

            // Reset ke Shot 1 (Start Point) jika ada
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

            // [PERBAIKAN] Loop menggunakan 'scenarios' yang kita ambil dari director
            for (const auto& point : scenarios)
            {
                CameraKeyframe key;
                key.isJumpCut = true;

                // [PERBAIKAN] Gunakan fungsi helper lokal 'GUICalculateRotation'
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

    // [PERBAIKAN] Loop scenarios
    for (int i = 0; i < scenarios.size(); ++i)
    {
        auto& pt = scenarios[i]; // Ambil reference biar bisa diedit
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
        // [PERBAIKAN] Push ke vector milik director
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

void GameBreakerGUI::DrawUI_LayoutTab(SceneGameBreaker* scene)
{
    ImGui::Spacing();
    ImGui::Text("Adjust In-Game UI Elements:");
    ImGui::Separator();

    // Mengakses member 'tutorialLayout' dari scene
    ImGuiEditPanel("Tutorial Panel",
        scene->tutorialLayout.x,
        scene->tutorialLayout.y,
        scene->tutorialLayout.scale,
        scene->tutorialLayout.lineSpacing,
        scene->tutorialLayout.color
    );
}

void GameBreakerGUI::DrawPostProcessTab(SceneGameBreaker* scene)
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

    auto& uber = scene->uberParams;

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
}


void GameBreakerGUI::DrawObjectColorTab(SceneGameBreaker* scene)
{
    ImGui::Spacing();

    // ===========================
    // SECTION: STAGE
    // ===========================
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

    // ===========================
    // SECTION: CHARACTER
    // ===========================
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CHARACTER");
    ImGui::Separator();
    ImGui::Spacing();

    // Paddle
    if (scene->paddle)
    {
        if (ImGui::CollapsingHeader("Paddle", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##Paddle", &scene->paddle->color.x);
            ImGui::Unindent();
        }
    }

    // Blocks
    if (scene->blockManager)
    {
        if (ImGui::CollapsingHeader("Blocks"))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##Blocks", &scene->blockManager->globalBlockColor.x);
            ImGui::Unindent();
        }
    }

    // Player
    if (scene->player)
    {
        if (ImGui::CollapsingHeader("Player"))
        {
            ImGui::Indent();
            ImGui::ColorEdit4("Base Color##Player", &scene->player->color.x);
            ImGui::Unindent();
        }
    }

    // ===========================
    // SECTION: ENEMIES
    // ===========================
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
}

void GameBreakerGUI::DrawSpriteTab(SceneGameBreaker* scene)
{
    ImGui::Begin("Sprite Inspector");
    if (ImGui::CollapsingHeader("World Decoration (Sprite 3D)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "TRANSFORM");
        ImGui::DragFloat3("Position", &scene->m_spritePos.x, 0.1f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SIZE");
        ImGui::DragFloat("Master Scale", &scene->m_spriteScale, 0.01f);
        ImGui::DragFloat2("Base Size W:H", &scene->m_spriteSize.x, 0.1f);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "ROTATION");
        ImGui::SliderFloat("Pitch", &scene->m_spritePitch, -180.0f, 180.0f);
        ImGui::SliderFloat("Yaw", &scene->m_spriteYaw, -180.0f, 180.0f);
        ImGui::SliderFloat("Roll", &scene->m_spriteRoll, -180.0f, 180.0f);
        ImGui::Unindent();
    }
    ImGui::End();
}

void GameBreakerGUI::ImGuiEditPanel(const char* label, float& x, float& y, float& scale, float& spacing, float* color)
{
    if (ImGui::TreeNode(label))
    {
        ImGui::DragFloat("Pos X", &x, 1.0f, 0.0f, 1920.0f);
        ImGui::DragFloat("Pos Y", &y, 1.0f, 0.0f, 1080.0f);
        ImGui::DragFloat("Scale", &scale, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Line Spacing", &spacing, 1.0f, 0.0f, 200.0f);
        ImGui::ColorEdit4("Color", color);
        ImGui::TreePop();
    }
}

void GameBreakerGUI::DrawObjectTransformTab(SceneGameBreaker* scene)
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
            ImGui::Indent();
            ImGui::TextDisabled("Edit debug boxes for collision setup.");

            // Iterate through the walls
            for (int i = 0; i < scene->m_stage->m_debugWalls.size(); ++i)
            {
                auto& wall = scene->m_stage->m_debugWalls[i];
                char label[32];
                snprintf(label, 32, "Wall #%d", i + 1);

                ImGui::PushID(i);
                if (ImGui::TreeNode(label))
                {
                    ImGui::DragFloat3("Pos", &wall.Position.x, 0.1f);
                    ImGui::DragFloat3("Rot", &wall.Rotation.x, 0.1f, -180.0f, 180.0f);
                    ImGui::DragFloat3("Scale", &wall.Scale.x, 0.05f, 0.0f, 100.0f);

                    if (ImGui::Button("Reset This Wall"))
                    {
                        // Check if there is a config default for this index
                        if (i < StageConfig::DEBUG_WALLS.size()) {
                            wall = StageConfig::DEBUG_WALLS[i];
                        }
                        else {
                            wall.Position = { 0,0,0 };
                            wall.Rotation = { 0,0,0 };
                            wall.Scale = StageConfig::WALL_DEFAULT_SCALE;
                        }
                    }

                    // --- NEW: Copy Value Button ---
                    ImGui::SameLine();
                    if (ImGui::Button("Copy Value"))
                    {
                        char buffer[256];
                        // Using %.6g to ensure exact value without trailing zeros or 'f'
                        snprintf(buffer, sizeof(buffer),
                            "// Wall %d\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
                            i + 1,
                            wall.Position.x, wall.Position.y, wall.Position.z,
                            wall.Rotation.x, wall.Rotation.y, wall.Rotation.z,
                            wall.Scale.x, wall.Scale.y, wall.Scale.z
                        );
                        ImGui::SetClipboardText(buffer);
                    }
                    // ------------------------------

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::Separator();

            // === NEW: ADD WALL BUTTON ===
            if (ImGui::Button("+ Add Wall Debug", ImVec2(-1, 30)))
            {
                scene->m_stage->AddDebugWall();
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Debug Line Transform (Lines)", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Lines are X-Axis aligned. Scale.X = Length.");

            for (int i = 0; i < scene->m_stage->m_debugLines.size(); ++i)
            {
                auto& line = scene->m_stage->m_debugLines[i];
                char label[32]; snprintf(label, 32, "Line #%d", i + 1);
                ImGui::PushID(i + 1000);
                if (ImGui::TreeNode(label))
                {
                    ImGui::DragFloat3("Pos", &line.Position.x, 0.1f);
                    ImGui::DragFloat3("Rot", &line.Rotation.x, 0.1f);
                    // Scale.X controls length
                    ImGui::DragFloat("Length (Scale.X)", &line.Scale.x, 0.1f);

                    // Copy Value (Formatted for StageConfig::DEBUG_LINES)
                    if (ImGui::Button("Copy Value")) {
                        char buffer[256];
                        snprintf(buffer, sizeof(buffer), "// Line %d\n{ {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g}, {%.6g,%.6g,%.6g} },",
                            i + 1, line.Position.x, line.Position.y, line.Position.z,
                            line.Rotation.x, line.Rotation.y, line.Rotation.z,
                            line.Scale.x, line.Scale.y, line.Scale.z);
                        ImGui::SetClipboardText(buffer);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset")) {
                        line.Position = { 0,0,0 }; line.Rotation = { 0,0,0 }; line.Scale = StageConfig::LINE_DEFAULT_SCALE;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("+ Add Line Debug")) scene->m_stage->AddDebugLine();
            ImGui::Unindent();
        }

        // 3. ENEMY TRANSFORMS (Existing Code)
        if (scene->m_enemyManager)
        {
            if (ImGui::CollapsingHeader("Enemies Transform", ImGuiTreeNodeFlags_None))
            {
                ImGui::Indent();

                auto& enemies = scene->m_enemyManager->GetEnemies();
                auto DrawEnemyUI = [](Enemy* e, int index, const char* label)
                    {
                        ImGui::PushID(index);

                        if (ImGui::TreeNode(label))
                        {
                            DirectX::XMFLOAT3 pos = e->GetPosition();
                            DirectX::XMFLOAT3 rot = e->GetRotation();

                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "POSITION");
                            if (ImGui::DragFloat3("XYZ##Pos", &pos.x, 0.1f)) e->SetPosition(pos);

                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "ROTATION");
                            if (ImGui::DragFloat3("Pitch/Yaw/Roll##Rot", &rot.x, 1.0f, -180.0f, 180.0f)) e->SetRotation(rot);

                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SCALE");
                            ImGui::DragFloat3("XYZ##Scl", &e->scale.x, 0.01f, 0.1f, 10.0f);

                            ImGui::Spacing();

                            if (ImGui::Button("Reset Transform"))
                            {
                                e->SetPosition(e->GetOriginalPosition());
                                e->SetRotation(e->GetOriginalRotation());
                                e->scale = { 1.0f, 1.0f, 1.0f };
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                        ImGui::Separator();
                    };

                int paddleCounter = 0;
                int ballCounter = 0;

                for (size_t i = 0; i < enemies.size(); ++i)
                {
                    Enemy* e = enemies[i].get();
                    if (e->GetType() == EnemyType::Paddle)
                    {
                        paddleCounter++;
                        char headerName[64];
                        snprintf(headerName, 64, "Enemy Paddle #%d", paddleCounter);
                        DrawEnemyUI(e, (int)i, headerName);
                    }
                }

                for (size_t i = 0; i < enemies.size(); ++i)
                {
                    Enemy* e = enemies[i].get();
                    if (e->GetType() == EnemyType::Ball)
                    {
                        ballCounter++;
                        char headerName[64];
                        snprintf(headerName, 64, "Enemy Ball #%d", ballCounter);
                        DrawEnemyUI(e, (int)i, headerName);
                    }
                }

                ImGui::Spacing();

                ImGui::Text("Spawn Controls:");
                if (ImGui::Button("+ Spawn Paddle"))
                {
                    EnemySpawnConfig newSpawn;
                    newSpawn.Position = { 0.0f, 0.0f, 0.0f };
                    newSpawn.Rotation = { 0.0f, 0.0f, 0.0f };
                    newSpawn.Color = { 1.0f, 0.2f, 0.2f, 1.0f };
                    newSpawn.Type = EnemyType::Paddle;
                    scene->m_enemyManager->SpawnEnemy(newSpawn);
                }

                ImGui::SameLine();

                if (ImGui::Button("+ Spawn Ball"))
                {
                    EnemySpawnConfig newSpawn;
                    newSpawn.Position = { 0.0f, 0.0f, 0.0f };
                    newSpawn.Rotation = { 0.0f, 0.0f, 0.0f };
                    newSpawn.Color = { 1.0f, 0.89f, 0.58f, 1.0f };
                    newSpawn.Type = EnemyType::Ball;
                    scene->m_enemyManager->SpawnEnemy(newSpawn);
                }

                ImGui::Unindent();
            }
        }
    }
}

