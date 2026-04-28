#include "BeyondWindow.h"
#include "System/Graphics.h"
#include "WindowManager.h" // Kita akan perbaiki ini nanti
#include <map>
#include <iostream>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Beyond
{
    namespace
    {
        std::map<HWND, WNDPROC> g_WindowProcMap;
        const UINT_PTR IDT_RESIZE_TIMER = 101;
    }

    LRESULT CALLBACK UnifiedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* pWindow = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        // Cegat Perintah Move (Drag Title Bar)
        if (msg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MOVE)
        {
            if (pWindow && !pWindow->IsDraggable()) return 0;
        }

        // ImGui Priority
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_ENTERSIZEMOVE:
            SetTimer(hWnd, IDT_RESIZE_TIMER, 1, NULL);
            return 0;

        case WM_EXITSIZEMOVE:
            KillTimer(hWnd, IDT_RESIZE_TIMER);
            return 0;

        case WM_TIMER:
            if (wParam == IDT_RESIZE_TIMER)
            {
                // Panggil melalui fungsi publik, bukan akses variabel private langsung
                if (pWindow)
                {
                    pWindow->TriggerTick();
                }
            }
            break;

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                WindowManager::Instance().HandleResize(hWnd, width, height);
            }
            break;
        }

        if (g_WindowProcMap.find(hWnd) != g_WindowProcMap.end())
        {
            return CallWindowProc(g_WindowProcMap[hWnd], hWnd, msg, wParam, lParam);
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    Window::Window()
    {
        // Alokasi memori dipindah ke Initialize
    }

    Window::~Window()
    {
        if (m_hWnd) g_WindowProcMap.erase(m_hWnd);
        if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);
    }

    bool Window::Initialize(const char* title, int width, int height, bool isTransparent) // <-- Tambahkan parameter
    {
        m_width = width;
        m_height = height;
        m_isTransparent = isTransparent; // <-- Simpan state-nya

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

        // [THE FIX] Beri tahu SDL apakah window ini transparan atau tidak
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, isTransparent);

        m_sdlWindow = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);

        if (!m_sdlWindow) return false;

        m_hWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(m_sdlWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

        // [THE FIX] Lempar statusnya ke Graphics
        Graphics::Instance().CreateSwapChain(m_hWnd, m_width, m_height, isTransparent, m_swapChain.GetAddressOf());
        CreateBuffers(m_width, m_height);

        WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)UnifiedWindowProc);
        if (oldProc) g_WindowProcMap[m_hWnd] = oldProc;

        return true;
    }

    void Window::CreateBuffers(int w, int h)
    {

        // Safety Check
        if (!m_swapChain) {
            OutputDebugStringA("Error: SwapChain is nullptr! Cannot create buffers.\n");
            return;
        }

        ID3D11Device* device = Graphics::Instance().GetDevice();
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
        device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = w; depthDesc.Height = h;
        depthDesc.MipLevels = 1; depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1; depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
        device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
        device->CreateDepthStencilView(depthTex.Get(), nullptr, m_depthStencilView.GetAddressOf());

        m_viewport.Width = (float)w; m_viewport.Height = (float)h;
        m_viewport.MinDepth = 0.0f; m_viewport.MaxDepth = 1.0f;
        m_viewport.TopLeftX = 0; m_viewport.TopLeftY = 0;
    }

    void Window::BeginRender(float r, float g, float b, float a)
    {
        float color[] = { r, g, b, 1.0f };
        auto context = Graphics::Instance().GetDeviceContext();
        context->ClearRenderTargetView(m_renderTargetView.Get(), color);
        context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
        context->RSSetViewports(1, &m_viewport);
    }

    void Window::EndRender(int syncInterval)
    {
        if (m_swapChain) m_swapChain->Present(syncInterval, 0);
    }

    void Window::Resize(int w, int h)
    {
        if (w <= 0 || h <= 0) return;
        if (w == m_width && h == m_height) return;

        // SAFETY CHECK
        if (!m_swapChain) return;

        m_width = w; m_height = h;

        auto context = Graphics::Instance().GetDeviceContext();
        context->OMSetRenderTargets(0, nullptr, nullptr);
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();
        context->Flush();

        m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        CreateBuffers(m_width, m_height);
    }

    void Window::SetVisible(bool visible)
    {
        m_isVisible = visible;
        if (visible) SDL_ShowWindow(m_sdlWindow);
        else SDL_HideWindow(m_sdlWindow);
    }

    void Window::SetTitle(const char* title)
    {
        if (m_sdlWindow) SDL_SetWindowTitle(m_sdlWindow, title);
    }

    void Window::SetTargetFPS(float fps)
    {
        if (fps > 0.0f) m_renderInterval = 1.0f / fps;
        else m_renderInterval = 0.0f;
    }

    bool Window::ShouldRender(float dt)
    {
        if (m_renderInterval <= 0.0f) return true;

        m_renderTimer += dt;
        if (m_renderTimer >= m_renderInterval)
        {
            m_renderTimer = 0.0f;
            return true;
        }
        return false;
    }
}