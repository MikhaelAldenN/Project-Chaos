#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "System/Sprite.h" 
#include "BitmapFont.h"
#include "Typewriter.h"
#include "ResourceManager.h"

class SceneIntroBios : public Scene
{
public:
    SceneIntroBios();
    ~SceneIntroBios() override = default;

    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void DrawGUI() override;
    void OnResize(int width, int height) override;

    Camera* GetCamera() const { return camera.get(); }

private:
    void SetupBiosLog();

    std::unique_ptr<Camera> camera;

    // Resources
    std::unique_ptr<Sprite> spriteLogoBoot;
    std::unique_ptr<Typewriter> biosLogSystem;

    // Logic Variables
    bool isExiting = false;       // Flag untuk fase black screen
    float exitTimer = 0.0f;       // Timer durasi black screen
    float finishDelayTimer = 0.0f; // Timer jeda setelah text selesai ketik
};