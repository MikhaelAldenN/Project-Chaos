#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi1_3.h>   // IDXGISwapChain2 untuk GetFrameLatencyWaitableObject
#include <dcomp.h>     // DirectComposition
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

        // BeginRender: untuk DComp window, tunggu frame latency waitable dulu
        void BeginRender(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
        void EndRender(int syncInterval = 1);
        void Resize(int width, int height);

        SDL_Window* GetSDLWindow()    const { return m_sdlWindow; }
        HWND        GetNativeHandle() const { return m_hWnd; }
        int         GetWidth()        const { return m_width; }
        int         GetHeight()       const { return m_height; }

        void    SetCamera(Camera* cam) { m_targetCamera = cam; }
        Camera* GetCamera()      const { return m_targetCamera; }

        void SetPriority(int p) { m_priority = p; }
        int  GetPriority()    const { return m_priority; }

        void SetVisible(bool visible);
        bool IsVisible() const { return m_isVisible; }

        void SetDraggable(bool enable) { m_isDraggable = enable; }
        bool IsDraggable()       const { return m_isDraggable; }

        void SetTitle(const char* title);

        // ── FPS Limiter (satu sistem, konsisten) ──────────────────────────
        // targetFPS <= 0 → tidak ada limit (present syncInterval menentukan)
        void SetFPSLimit(float fps) {
            m_targetFPS = fps;
            m_frameInterval = (fps > 0.0f) ? (1.0f / fps) : 0.0f;
        }
        // Legacy alias
        void SetTargetFPS(float fps) { SetFPSLimit(fps); }

        bool ShouldRender(float dt);

        using TickCallback = std::function<void()>;
        void SetTickCallback(TickCallback callback) { m_tickCallback = callback; }
        void TriggerTick() { if (m_tickCallback) m_tickCallback(); }

        void SetClickThrough(bool enable);
        bool IsClickThrough() const { return m_isClickThrough; }

        void SetBorderVisible(bool visible) { m_showBorder = visible; }
        bool IsBorderVisible()       const { return m_showBorder; }

        void  SetBackgroundAlpha(float alpha) { m_backgroundAlpha = alpha; }
        float GetBackgroundAlpha()      const { return m_backgroundAlpha; }

        // ── State yang perlu diakses dari WndProc / WindowManager ────────
        bool m_isClickThrough = false;
        bool m_showBorder = true;
        int  m_lastRenderedW = 0;
        int  m_lastRenderedH = 0;

        // Dipanggil SEKALI per frame dari WindowManager, sebelum loop render.
        // Jangan panggil dari dalam BeginRender — menyebabkan stall per window!
        void WaitFrameLatency()
        {
            if (!m_frameLatencyHandle) return;
            DWORD result;
            // Loop sampai benar-benar siap (bukan sekadar APC interrupt)
            do {
                result = WaitForSingleObjectEx(m_frameLatencyHandle, 100, TRUE);
            } while (result == WAIT_IO_COMPLETION);
            // WAIT_TIMEOUT → lanjutkan saja, jangan blok total
        }

        void ScheduleHitTestReadback();   // dipanggil setelah Present
        float SampleAlphaAt(int screenX, int screenY); // dipanggil dari WndProc

    private:
        // Buat RTV + DSV dari swap chain back buffer (dipakai kedua mode)
        void CreateBuffers(int w, int h);

        // Inisialisasi DirectComposition visual tree
        void InitDComp();

        // Render border tipis sebagai 4 quad D3D (hanya transparent mode)
        // TODO: hubungkan ke ShapeRenderer kalau sudah tersedia
        void RenderBorderD3D();

        TickCallback m_tickCallback = nullptr;

        float m_backgroundAlpha = 1.0f;
        float m_targetFPS = 60.0f;
        float m_frameInterval = 0.0f;   // 1/fps
        float m_accumulatedTime = 0.0f;   // untuk ShouldRender

        Microsoft::WRL::ComPtr<ID3D11Texture2D>  m_hitTestStaging;  // CPU-readable copy
        int  m_stagingW = 0, m_stagingH = 0;
        bool m_stagingReady = false;

    private:
        SDL_Window* m_sdlWindow = nullptr;
        HWND        m_hWnd = nullptr;

        int  m_width = 0;
        int  m_height = 0;
        int  m_priority = 100;
        bool m_isVisible = true;
        bool m_isDraggable = true;
        bool m_isTransparent = false;

        Camera* m_targetCamera = nullptr;

        // ── Swap chain (dipakai KEDUA mode — normal & transparent) ────────
        Microsoft::WRL::ComPtr<IDXGISwapChain1>        m_swapChain;

        // Handle waitable: CPU block sampai GPU siap frame berikutnya
        // → mengurangi input latency, hindari CPU spin-wait manual
        HANDLE m_frameLatencyHandle = nullptr;

        // ── DirectComposition (hanya transparent window) ──────────────────
        Microsoft::WRL::ComPtr<IDCompositionDevice>  m_dcompDevice;
        Microsoft::WRL::ComPtr<IDCompositionTarget>  m_dcompTarget;
        Microsoft::WRL::ComPtr<IDCompositionVisual>  m_dcompVisual;

        // ── Shared render resources ───────────────────────────────────────
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
        D3D11_VIEWPORT m_viewport = {};
    };
}