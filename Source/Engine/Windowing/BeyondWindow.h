#pragma once

// Lean and mean untuk mengurangi beban kompilasi dari Windows API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <functional>

// Forward declaration (Lebih profesional daripada include file header lain jika hanya butuh pointer)
class Camera;

namespace Beyond
{
    class Window
    {
    public:
        // Konstruktor kosong, alokasi dilakukan di Initialize
        Window();
        ~Window();

        // Mengembalikan true jika berhasil, false jika gagal
        bool Initialize(const char* title, int width, int height, bool isTransparent = false);
        bool IsTransparent() const { return m_isTransparent; } // <-- Tambahkan Getter ini

        // Window Management
        void BeginRender(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);        
        void EndRender(int syncInterval = 1);
        void Resize(int width, int height);

        // Getters
        SDL_Window* GetSDLWindow() const { return m_sdlWindow; }
        HWND GetNativeHandle() const { return m_hWnd; } // Ganti nama agar lebih universal
        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }

        void SetCamera(Camera* cam) { m_targetCamera = cam; }
        Camera* GetCamera() const { return m_targetCamera; }

        void SetPriority(int p) { m_priority = p; }
        int GetPriority() const { return m_priority; }

        void SetVisible(bool visible);
        bool IsVisible() const { return m_isVisible; }

        void SetDraggable(bool enable) { m_isDraggable = enable; }
        bool IsDraggable() const { return m_isDraggable; }
        void SetTitle(const char* title);

        void SetTargetFPS(float fps);
        bool ShouldRender(float dt);
        using TickCallback = std::function<void()>;
        void SetTickCallback(TickCallback callback) { m_tickCallback = callback; }

        void TriggerTick() { if (m_tickCallback) m_tickCallback(); }

    private:
        // Pindahkan fungsi privat ke bawah
        void CreateBuffers(int w, int h);

        TickCallback m_tickCallback = nullptr;

    private:
        // Standar penamaan member class profesional: gunakan awalan 'm_'
        SDL_Window* m_sdlWindow = nullptr;
        HWND m_hWnd = nullptr;

        int m_width = 0;
        int m_height = 0;
        int m_priority = 100;
        bool m_isVisible = true;
        bool m_isDraggable = true;
        bool m_isTransparent = false; 

        float m_renderInterval = 0.0f;
        float m_renderTimer = 0.0f;

        Camera* m_targetCamera = nullptr;

        // DirectX Resources
        Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_depthStencilView;
        D3D11_VIEWPORT                                  m_viewport = {};
    };
}