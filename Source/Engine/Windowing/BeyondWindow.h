#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <functional>

class Camera;

namespace Beyond
{
    class Window
    {
    public:
        Window();
        ~Window();

        bool Initialize(const char* title, int width, int height, bool isTransparent = false);
        bool IsTransparent() const { return m_isTransparent; }

        void BeginRender(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
        void EndRender(int syncInterval = 1);
        void Resize(int width, int height);

        SDL_Window* GetSDLWindow() const { return m_sdlWindow; }
        HWND GetNativeHandle() const { return m_hWnd; }
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

        // Tambahkan di bagian public:
        void SetClickThrough(bool enable);
        bool IsClickThrough() const { return m_isClickThrough; }

        void SetBorderVisible(bool visible) { m_showBorder = visible; }
        bool IsBorderVisible() const { return m_showBorder; }

        void  SetBackgroundAlpha(float alpha) { m_backgroundAlpha = alpha; }
        float GetBackgroundAlpha() const { return m_backgroundAlpha; }

        // Tambahkan di bagian private variables (di bawah m_isTransparent):
        bool m_isClickThrough = false;
        bool m_showBorder = true;

    private:
        void CreateBuffers(int w, int h);
        void CreateOffscreenBuffers(int w, int h);
        void RecreateLayeredSurface(int w, int h);
        void DrawBorderOnBitmap();
        void UpdateLayeredSurface();

        TickCallback m_tickCallback = nullptr;

        float m_backgroundAlpha = 1.0f;

    private:
        SDL_Window* m_sdlWindow = nullptr;
        HWND        m_hWnd = nullptr;

        int  m_width = 0;
        int  m_height = 0;
        int  m_priority = 100;
        bool m_isVisible = true;
        bool m_isDraggable = true;
        bool m_isTransparent = false;

        float m_renderInterval = 0.0f;
        float m_renderTimer = 0.0f;

        Camera* m_targetCamera = nullptr;

        // Normal window
        Microsoft::WRL::ComPtr<IDXGISwapChain1>        m_swapChain;

        // Shared
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
        D3D11_VIEWPORT                                 m_viewport = {};

        // Transparent window: offscreen path
        Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_offscreenTex;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_stagingTex;

        // GDI Layered Window
        HDC     m_hdcScreen = nullptr;
        HDC     m_hdcMem = nullptr;
        HBITMAP m_hBitmap = nullptr;
        void* m_pBits = nullptr;
        int     m_layeredW = 0;
        int     m_layeredH = 0;
    };
}