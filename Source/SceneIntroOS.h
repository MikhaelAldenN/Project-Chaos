#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "System/Sprite.h" 
#include "Primitive.h"
#include "BitmapFont.h"
#include "ResourceManager.h"

enum class OSState
{
    AnimatingLogo,  // Logo sedang bergerak masuk
    ShowText,       // Logo diam, teks muncul
    Finished        // Siap ganti scene
};

class SceneIntroOS : public Scene
{
public:
    SceneIntroOS();
    ~SceneIntroOS() override = default;

    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void DrawGUI() override; // Kosong, tapi wajib override
    void OnResize(int width, int height) override;

    Camera* GetCamera() const { return camera.get(); }

private:
    std::unique_ptr<Camera> camera;

    // Resources
    std::unique_ptr<Sprite> spriteOSLogo;
    std::unique_ptr<Primitive> primitiveBatch;

    // Logic Timers
    float timer = 0.0f;
    float blinkTimer = 0.0f;
    float animTimer = 0.0f;

    OSState currentState = OSState::AnimatingLogo;

    // Animation State Variables
    // (Kita simpan di sini agar nilainya persisten antar-frame saat Update)
    float logoTargetX = 0.0f;

    // Logo 1 State
    float logo1_StartX = 0.0f;
    float logo1_CurrentX = 0.0f;
    float logo1_Y = 0.0f;

    // Logo 2 State
    float logo2_StartX = 0.0f;
    float logo2_CurrentX = 0.0f;
    float logo2_Y = 0.0f;
};