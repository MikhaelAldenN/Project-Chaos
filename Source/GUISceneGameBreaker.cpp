#include "GUISceneGameBreaker.h"
#include "SceneGameBreaker.h" // Wajib include definisi full Scene di sini
#include "CameraController.h"
#include <imgui.h>
#include <vector>
#include <string>
#include "CinematicDirector.h"

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
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CHARACTER & OBJECTS");
    ImGui::Separator();
    ImGui::Spacing();

    if (scene->paddle && ImGui::CollapsingHeader("Paddle", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::ColorEdit4("Color##P", &scene->paddle->color.x);
        ImGui::Unindent();
    }

    if (scene->blockManager && ImGui::CollapsingHeader("Blocks")) {
        ImGui::Indent();
        ImGui::ColorEdit4("Color##B", &scene->blockManager->globalBlockColor.x);
        ImGui::Unindent();
    }

    if (scene->player && ImGui::CollapsingHeader("Player")) {
        ImGui::Indent();
        ImGui::ColorEdit4("Color##Pl", &scene->player->color.x);
        ImGui::Unindent();
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