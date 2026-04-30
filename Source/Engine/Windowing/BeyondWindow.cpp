#include "BeyondWindow.h"
#include "System/Graphics.h"
#include "WindowManager.h"
#include <map>
#include <windowsx.h>
#include "PerformanceLogger.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Beyond
{
    namespace
    {
        std::map<HWND, WNDPROC> g_WindowProcMap;
        const UINT_PTR IDT_RESIZE_TIMER = 101;

        // [REFACTOR] Warna Kuning
        constexpr uint8_t BORDER_B = 0;   // Blue = 0
        constexpr uint8_t BORDER_G = 255; // Green = 255
        constexpr uint8_t BORDER_R = 255; // Red = 255
        constexpr uint8_t BORDER_A = 180; // Opacity border sedikit dinaikkan
        constexpr int     BORDER_WIDTH = 2;
    }

    LRESULT CALLBACK UnifiedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* pWindow = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        if (msg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MOVE)
        {
            if (pWindow && !pWindow->IsDraggable()) return 0;
        }

        // =========================================================
                // [FIX] LOGIKA DRAG & RESIZE UNTUK TRANSPARENT WINDOW
                // =========================================================
        if (msg == WM_NCHITTEST && pWindow && pWindow->IsTransparent())
        {
            if (pWindow->IsClickThrough()) return HTTRANSPARENT;

            RECT rc;
            GetWindowRect(hWnd, &rc);
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            const int BORDER_HIT_AREA = 8;

            // [FIX] Bedakan Hollow vs Solid berdasarkan Alpha!
            bool isSolidMode = (pWindow->GetBackgroundAlpha() > 0.0f);
            const int TITLEBAR_HEIGHT = isSolidMode ? 0 : 24;

            bool isLeft = (x >= rc.left && x < rc.left + BORDER_HIT_AREA);
            bool isRight = (x < rc.right && x >= rc.right - BORDER_HIT_AREA);
            bool isTop = (y >= rc.top && y < rc.top + BORDER_HIT_AREA);
            bool isBottom = (y < rc.bottom && y >= rc.bottom - BORDER_HIT_AREA);

            // 1. Cek pojokan (Corner resize)
            if (isTop && isLeft)     return HTTOPLEFT;
            if (isTop && isRight)    return HTTOPRIGHT;
            if (isBottom && isLeft)  return HTBOTTOMLEFT;
            if (isBottom && isRight) return HTBOTTOMRIGHT;

            // 2. Cek sisi pinggir (Edge resize)
            if (isLeft)   return HTLEFT;
            if (isRight)  return HTRIGHT;
            if (isTop)    return HTTOP;
            if (isBottom) return HTBOTTOM;

            // 3. Cek area Titlebar (Hanya berlaku untuk Hollow Mode)
            bool isTitlebarArea = (!isSolidMode && y >= rc.top && y < rc.top + TITLEBAR_HEIGHT);
            if (isTitlebarArea && pWindow->IsDraggable()) return HTCAPTION;

            // 4. Sisa area tengah
            // Jika ini Solid Mode (Alpha > 0), sulap area tengahnya jadi drag zone!
            if (isSolidMode && pWindow->IsDraggable()) return HTCAPTION;

            // Jika Hollow, biarkan tembus
            return HTCLIENT;
        }

        // =========================================================
        // [FIX] KURSOR DRAG UNTUK AREA HTCAPTION
        // =========================================================
        if (msg == WM_SETCURSOR)
        {
            WORD hitTest = LOWORD(lParam); // Ambil hasil hit-test (HTCAPTION, HTLEFT, dll)

            // Jika kursor ada di area tengah (HTCAPTION) pada window transparan yang draggable
            if (hitTest == HTCAPTION && pWindow && pWindow->IsTransparent() && pWindow->IsDraggable())
            {
                // Ganti kursor jadi panah 4 arah
                SetCursor(LoadCursor(NULL, IDC_SIZEALL));

                // Return TRUE buat ngasih tau Windows: 
                // "Kursornya udah gue atur, jangan ditimpa pakai panah default!"
                return TRUE;
            }
        }

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
            if (wParam == IDT_RESIZE_TIMER && pWindow)
                pWindow->TriggerTick();
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
            return CallWindowProc(g_WindowProcMap[hWnd], hWnd, msg, wParam, lParam);

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    Window::Window() {}

    Window::~Window()
    {
        if (m_hWnd) g_WindowProcMap.erase(m_hWnd);
        if (m_hBitmap) { DeleteObject(m_hBitmap);      m_hBitmap = nullptr; }
        if (m_hdcMem) { DeleteDC(m_hdcMem);           m_hdcMem = nullptr; }
        if (m_hdcScreen) { ReleaseDC(NULL, m_hdcScreen); m_hdcScreen = nullptr; }
        if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);
    }

    bool Window::Initialize(const char* title, int width, int height, bool isTransparent)
    {
        m_width = width;
        m_height = height;
        m_isTransparent = isTransparent;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, false);

        m_sdlWindow = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        if (!m_sdlWindow) return false;

        m_hWnd = (HWND)SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_sdlWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

        if (isTransparent)
        {
            // Hapus border dan titlebar OS visual sepenuhnya dari bitmap kita
            LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            SetWindowLong(m_hWnd, GWL_STYLE, style);

            // WS_EX_LAYERED untuk UpdateLayeredWindow
            // WS_EX_TOOLWINDOW agar tidak muncul di taskbar
            LONG exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_LAYERED | WS_EX_TOOLWINDOW;
            SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);

            SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

            m_hdcScreen = GetDC(NULL);
            m_hdcMem = CreateCompatibleDC(m_hdcScreen);
            RecreateLayeredSurface(width, height);
            CreateOffscreenBuffers(width, height);
        }
        else
        {
            Graphics::Instance().CreateSwapChain(m_hWnd, width, height, false, m_swapChain.GetAddressOf());
            CreateBuffers(width, height);
        }

        WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(
            m_hWnd, GWLP_WNDPROC, (LONG_PTR)UnifiedWindowProc);
        if (oldProc) g_WindowProcMap[m_hWnd] = oldProc;

        return true;
    }

    void Window::CreateOffscreenBuffers(int w, int h)
    {
        ID3D11Device* device = Graphics::Instance().GetDevice();
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();
        m_offscreenTex.Reset();
        m_stagingTex.Reset();

        D3D11_TEXTURE2D_DESC rtDesc = {};
        rtDesc.Width = w; rtDesc.Height = h;
        rtDesc.MipLevels = 1; rtDesc.ArraySize = 1;
        rtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        rtDesc.SampleDesc.Count = 1;
        rtDesc.Usage = D3D11_USAGE_DEFAULT;
        rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&rtDesc, nullptr, m_offscreenTex.GetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed to create offscreen RT!\n");
            return;
        }
        device->CreateRenderTargetView(m_offscreenTex.Get(), nullptr, m_renderTargetView.GetAddressOf());

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = w; depthDesc.Height = h;
        depthDesc.MipLevels = 1; depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
        device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
        device->CreateDepthStencilView(depthTex.Get(), nullptr, m_depthStencilView.GetAddressOf());

        D3D11_TEXTURE2D_DESC stagingDesc = {};
        stagingDesc.Width = w; stagingDesc.Height = h;
        stagingDesc.MipLevels = 1; stagingDesc.ArraySize = 1;
        stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        device->CreateTexture2D(&stagingDesc, nullptr, m_stagingTex.GetAddressOf());

        m_viewport.Width = (float)w; m_viewport.Height = (float)h;
        m_viewport.MinDepth = 0.0f; m_viewport.MaxDepth = 1.0f;
        m_viewport.TopLeftX = 0; m_viewport.TopLeftY = 0;
    }

    void Window::CreateBuffers(int w, int h)
    {
        if (!m_swapChain) { OutputDebugStringA("Error: SwapChain nullptr!\n"); return; }
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
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
        device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
        device->CreateDepthStencilView(depthTex.Get(), nullptr, m_depthStencilView.GetAddressOf());

        m_viewport.Width = (float)w; m_viewport.Height = (float)h;
        m_viewport.MinDepth = 0.0f; m_viewport.MaxDepth = 1.0f;
        m_viewport.TopLeftX = 0; m_viewport.TopLeftY = 0;
    }

    void Window::RecreateLayeredSurface(int w, int h)
    {
        if (m_hBitmap)
        {
            SelectObject(m_hdcMem, (HBITMAP)NULL);
            DeleteObject(m_hBitmap);
            m_hBitmap = nullptr;
            m_pBits = nullptr;
        }
        BITMAPINFOHEADER bih = {};
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = w; bih.biHeight = -h;
        bih.biPlanes = 1; bih.biBitCount = 32;
        bih.biCompression = BI_RGB;
        BITMAPINFO bi = {}; bi.bmiHeader = bih;
        m_hBitmap = CreateDIBSection(m_hdcMem, &bi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
        SelectObject(m_hdcMem, m_hBitmap);
        m_layeredW = w;
        m_layeredH = h;
    }

    //void Window::DrawBorderOnBitmap()
    //{
    //    if (!m_pBits || m_layeredW <= 0 || m_layeredH <= 0) return;
    //    uint8_t* dst = (uint8_t*)m_pBits;
    //    const int stride = m_layeredW * 4;
    //    for (int y = 0; y < m_layeredH; y++)
    //    {
    //        for (int x = 0; x < m_layeredW; x++)
    //        {
    //            bool isBorder = (x < BORDER_WIDTH || x >= m_layeredW - BORDER_WIDTH ||
    //                y < BORDER_WIDTH || y >= m_layeredH - BORDER_WIDTH);
    //            if (!isBorder) continue;
    //            uint8_t* px = dst + y * stride + x * 4;
    //            px[0] = BORDER_B;
    //            px[1] = BORDER_G;
    //            px[2] = BORDER_R;
    //            px[3] = BORDER_A;
    //        }
    //    }
    //}

    void Window::UpdateLayeredSurface()
    {
        PerformanceLogger::Instance().StartTimer(PerfBucket::WindowOS); // START SINI
        if (!m_offscreenTex || !m_stagingTex || !m_pBits) return;
        auto context = Graphics::Instance().GetDeviceContext();

        context->CopyResource(m_stagingTex.Get(), m_offscreenTex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(context->Map(m_stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            return;

        uint8_t* src = (uint8_t*)mapped.pData;
        uint8_t* dst = (uint8_t*)m_pBits;
        for (int y = 0; y < m_layeredH; y++)
            memcpy(dst + y * (m_layeredW * 4), src + y * mapped.RowPitch, m_layeredW * 4);

        context->Unmap(m_stagingTex.Get(), 0);

        // Gambar border 2px putih di atas hasil render
        DrawBorderOnBitmap();

        POINT ptSrc = { 0, 0 };
        SIZE  sz = { m_layeredW, m_layeredH };
        int winX = 0, winY = 0;
        SDL_GetWindowPosition(m_sdlWindow, &winX, &winY);
        POINT ptDst = { (LONG)winX, (LONG)winY };

        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(m_hWnd, m_hdcScreen, &ptDst, &sz,
            m_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

        PerformanceLogger::Instance().StopTimer(PerfBucket::WindowOS); // STOP SINI
    }

    void Window::BeginRender(float r, float g, float b, float a)
    {
        float color[] = { r, g, b, a };
        auto context = Graphics::Instance().GetDeviceContext();
        context->ClearRenderTargetView(m_renderTargetView.Get(), color);
        context->ClearDepthStencilView(m_depthStencilView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(),
            m_depthStencilView.Get());
        context->RSSetViewports(1, &m_viewport);
    }

    void Window::EndRender(int syncInterval)
    {
        if (m_isTransparent)
            UpdateLayeredSurface();
        else
            if (m_swapChain) m_swapChain->Present(syncInterval, 0);
    }

    void Window::Resize(int w, int h)
    {
        if (w <= 0 || h <= 0) return;
        if (w == m_width && h == m_height) return;
        m_width = w; m_height = h;

        auto context = Graphics::Instance().GetDeviceContext();
        context->OMSetRenderTargets(0, nullptr, nullptr);
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();
        context->Flush();

        if (m_isTransparent)
        {
            RecreateLayeredSurface(w, h);
            CreateOffscreenBuffers(w, h);
        }
        else
        {
            m_swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
            CreateBuffers(w, h);
        }
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

    // =========================================================
    // [FIX] IMPLEMENTASI SETTER BARU
    // =========================================================
    void Window::SetClickThrough(bool enable)
    {
        m_isClickThrough = enable;
        if (m_hWnd)
        {
            LONG exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
            if (enable)
                exStyle |= WS_EX_TRANSPARENT; // Bikin click tembus
            else
                exStyle &= ~WS_EX_TRANSPARENT; // Balikin jadi normal solid

            SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);
        }
    }

    void Window::DrawBorderOnBitmap()
    {
        if (!m_showBorder || !m_pBits || m_layeredW <= 0 || m_layeredH <= 0) return;

        uint8_t* dst = (uint8_t*)m_pBits;
        const int stride = m_layeredW * 4;

        // [FIX] Cek tipe window untuk menentukan titlebar
        bool isSolidMode = (m_backgroundAlpha > 0.0f);
        const int TITLEBAR_HEIGHT = isSolidMode ? 0 : 24;

        for (int y = 0; y < m_layeredH; y++)
        {
            for (int x = 0; x < m_layeredW; x++)
            {
                bool isTitlebar = (!isSolidMode && y < TITLEBAR_HEIGHT);

                // [FIX] Kembalikan logika border atas (y < BORDER_WIDTH) agar 
                // Solid Mode tetap punya garis kuning pembatas di bagian atasnya.
                bool isBorder = (x < BORDER_WIDTH || x >= m_layeredW - BORDER_WIDTH ||
                    y < BORDER_WIDTH || y >= m_layeredH - BORDER_WIDTH);

                if (!isTitlebar && !isBorder) continue;

                uint8_t* px = dst + y * stride + x * 4;
                px[0] = BORDER_B;
                px[1] = BORDER_G;
                px[2] = BORDER_R;
                px[3] = isTitlebar ? 220 : BORDER_A;
            }
        }
    }
}