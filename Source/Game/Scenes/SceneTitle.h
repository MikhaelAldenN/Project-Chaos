#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Scene.h"
#include "Camera.h"
#include "System/AudioManager.h"
#include "System/Sprite.h" 
#include "PostProcessManager.h"
#include "SceneGame.h"
#include "Framework.h"
#include "ResourceManager.h"
#include "System/Input.h"
#include "System/Graphics.h"
#include <imgui.h>

class SceneTitle : public Scene
{
public:
    SceneTitle();
    ~SceneTitle() override = default;

    // Core Loop
    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void OnResize(int width, int height) override;

    // Debug / Tools
    void DrawGUI() override;
    Camera* GetCamera() const { return camera.get(); }

private:
    // --- Subsystems ---
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Sprite> bgSprite;
    std::unique_ptr<Sprite> logoSprite;
    std::unique_ptr<Sprite> copyrightSprite;
    std::unique_ptr<PostProcessManager> postProcess;

    // --- Post Process State ---
    struct PostProcessState {
        bool MasterEnabled{ true };
        bool EnableVignette{ false };
        bool EnableLens{ true };
        bool EnableChromatic{ true };
        bool EnableCRT{ true };
        bool EnableBloom{ true };
        bool EnablePSX{ true };
    };

    PostProcessState m_fxState{};
    UberShader::UberData m_uberParams{};

    std::unique_ptr<Sprite> m_fadeSprite{};
    std::unique_ptr<Sprite> startSprite; 
    float m_fadeAlpha{ 1.0f };
    float m_bootTimer{ 4.1f };
    float m_copyrightTimer{ 4.0f }; 
    float m_copyrightAlpha{ 1.0f };
    float m_startAlpha{ 0.0f };
    float m_pulseTimer{ 0.0f };
    static constexpr float BOOT_FADE_DURATION{ 3.0f };

    bool m_isExiting{ false };
    float m_exitTimer{ 0.0f };

    static constexpr float FX_BASE_SMOOTHNESS{ 0.2f };
    static constexpr float FX_BASE_INTENSITY{ 0.38f };

    // --- Debug GUI Helpers ---
    void GUIPostProcessTab();
};