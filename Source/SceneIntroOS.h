#pragma once

#include <memory>
#include <wrl/client.h>
#include "Scene.h"
#include "Camera.h"
#include "System/Sprite.h" 
#include "Primitive.h"
#include "BitmapFont.h"
#include "ResourceManager.h"
#include "UberShader.h"

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
    void CreateRenderTarget(); 
    void GUIPostProcessTab();  

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

    // GUI State
    struct PostProcessState
    {
        bool MasterEnabled = true;
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };
    PostProcessState m_fxState;

    // Post-Processing Resources
    std::unique_ptr<UberShader> uberShader;
    UberShader::UberData uberParams;
    float m_globalTime = 0.0f;

    // DirectX Render Targets
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
};