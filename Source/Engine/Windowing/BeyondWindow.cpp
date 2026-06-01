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

        constexpr uint8_t BORDER_B = 0;
        constexpr uint8_t BORDER_G = 255;
        constexpr uint8_t BORDER_R = 255;
        constexpr uint8_t BORDER_A = 180;
        constexpr int     BORDER_WIDTH = 2;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // WndProc — tidak berubah dari versi sebelumnya
    // ─────────────────────────────────────────────────────────────────────────
    LRESULT CALLBACK UnifiedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* pWindow = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        if (msg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MOVE)
        {
            if (pWindow && !pWindow->IsDraggable()) return 0;
        }

        if (msg == WM_NCHITTEST && pWindow && pWindow->IsTransparent())
        {
            int screenX = GET_X_LPARAM(lParam);
            int screenY = GET_Y_LPARAM(lParam);

            // --- PER-PIXEL HIT TEST ---
            // Alpha < 0.05 = transparan → click lewat ke window di belakang
            float alpha = pWindow->SampleAlphaAt(screenX, screenY);
            if (alpha < 0.05f) return HTTRANSPARENT;

            // Kalau opaque, lanjut ke hit test normal (resize handles, drag, dll)
            if (pWindow->IsClickThrough()) return HTTRANSPARENT;

            RECT rc;
            GetWindowRect(hWnd, &rc);
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            const int BORDER_HIT_AREA = 8;
            bool isSolidMode = (pWindow->GetBackgroundAlpha() > 0.0f);
            const int TITLEBAR_HEIGHT = isSolidMode ? 0 : 24;

            bool isLeft = (x >= rc.left && x < rc.left + BORDER_HIT_AREA);
            bool isRight = (x < rc.right && x >= rc.right - BORDER_HIT_AREA);
            bool isTop = (y >= rc.top && y < rc.top + BORDER_HIT_AREA);
            bool isBottom = (y < rc.bottom && y >= rc.bottom - BORDER_HIT_AREA);

            if (isTop && isLeft)     return HTTOPLEFT;
            if (isTop && isRight)    return HTTOPRIGHT;
            if (isBottom && isLeft)  return HTBOTTOMLEFT;
            if (isBottom && isRight) return HTBOTTOMRIGHT;
            if (isLeft)   return HTLEFT;
            if (isRight)  return HTRIGHT;
            if (isTop)    return HTTOP;
            if (isBottom) return HTBOTTOM;

            bool isTitlebarArea = (!isSolidMode && y >= rc.top && y < rc.top + TITLEBAR_HEIGHT);
            if (isTitlebarArea && pWindow->IsDraggable()) return HTCAPTION;
            if (isSolidMode && pWindow->IsDraggable())    return HTCAPTION;
            return HTCLIENT;
        }

        if (msg == WM_SETCURSOR)
        {
            WORD hitTest = LOWORD(lParam);
            if (hitTest == HTCAPTION && pWindow && pWindow->IsTransparent() && pWindow->IsDraggable())
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEALL));
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

    // ─────────────────────────────────────────────────────────────────────────
    // Constructor / Destructor
    // ─────────────────────────────────────────────────────────────────────────
    Window::Window() {}

    Window::~Window()
    {
        // Lepaskan frame latency handle sebelum swap chain dihancurkan
        if (m_frameLatencyHandle)
        {
            CloseHandle(m_frameLatencyHandle);
            m_frameLatencyHandle = nullptr;
        }

        // DComp harus dilepas sebelum swap chain
        m_dcompVisual.Reset();
        m_dcompTarget.Reset();
        m_dcompDevice.Reset();

        if (m_hWnd) g_WindowProcMap.erase(m_hWnd);
        if (m_sdlWindow) SDL_DestroyWindow(m_sdlWindow);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Initialize
    // ─────────────────────────────────────────────────────────────────────────
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

        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true); // ← tambah ini
        m_sdlWindow = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        if (!m_sdlWindow) return false;

        m_hWnd = (HWND)SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_sdlWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

        if (isTransparent)
        {
            // ── Window style: hilangkan chrome OS, aktifkan layered ────────
            LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            SetWindowLong(m_hWnd, GWL_STYLE, style);

            LONG exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;
            // WS_EX_NOREDIRECTIONBITMAP: beritahu DWM bahwa kita pakai DComp,
            // bukan GDI redirection surface → wajib ada untuk DComp bekerja
            exStyle |= WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED;
            SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);

            SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

            // ── Buat swap chain untuk DComp (bukan untuk HWND) ────────────
            Graphics::Instance().CreateSwapChainForComposition(
                width, height, m_swapChain.GetAddressOf());

            // ── Inisialisasi DirectComposition visual tree ─────────────────
            InitDComp();
        }
        else
        {
            // ── Normal window: swap chain langsung ke HWND ─────────────────
            Graphics::Instance().CreateSwapChainForHwnd(
                m_hWnd, width, height, m_swapChain.GetAddressOf());
        }

        // ── Ambil frame latency waitable handle ───────────────────────────
        {
            Microsoft::WRL::ComPtr<IDXGISwapChain2> sc2;
            if (SUCCEEDED(m_swapChain.As(&sc2)))
                m_frameLatencyHandle = sc2->GetFrameLatencyWaitableObject();
        }

        // ── Buat RTV + DSV dari back buffer ───────────────────────────────
        CreateBuffers(width, height);

        if (m_sdlWindow) SDL_ShowWindow(m_sdlWindow);


        // ── Hook WndProc ──────────────────────────────────────────────────
        WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(
            m_hWnd, GWLP_WNDPROC, (LONG_PTR)UnifiedWindowProc);
        if (oldProc) g_WindowProcMap[m_hWnd] = oldProc;

        return true;

    }

    // ─────────────────────────────────────────────────────────────────────────
    // InitDComp — sambungkan swap chain ke HWND lewat DComp visual tree
    //
    // Aliran data (semuanya di GPU, nol CPU readback):
    //   GPU render → swap chain back buffer → Present()
    //   DWM compositor membaca swap chain via IDCompositionVisual → tampil
    // ─────────────────────────────────────────────────────────────────────────
    void Window::InitDComp()
    {
        IDXGIDevice* dxgiDevice = Graphics::Instance().GetDXGIDevice();

        HRESULT hr = DCompositionCreateDevice(
            dxgiDevice,
            __uuidof(IDCompositionDevice),
            (void**)m_dcompDevice.GetAddressOf());
        if (FAILED(hr))
        {
            OutputDebugStringA("[DComp] DCompositionCreateDevice FAILED\n");
            return;
        }

        // Target = "tujuan compositing" → HWND ini
        hr = m_dcompDevice->CreateTargetForHwnd(m_hWnd, TRUE, m_dcompTarget.GetAddressOf());
        if (FAILED(hr)) { OutputDebugStringA("[DComp] CreateTargetForHwnd FAILED\n"); return; }

        // Visual = node dalam scene graph DComp
        hr = m_dcompDevice->CreateVisual(m_dcompVisual.GetAddressOf());
        if (FAILED(hr)) { OutputDebugStringA("[DComp] CreateVisual FAILED\n"); return; }

        // Bind swap chain sebagai konten visual
        hr = m_dcompVisual->SetContent(m_swapChain.Get());
        if (FAILED(hr)) { OutputDebugStringA("[DComp] SetContent FAILED\n"); return; }

        // Set visual sebagai root dari target HWND
        hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
        if (FAILED(hr)) { OutputDebugStringA("[DComp] SetRoot FAILED\n"); return; }

        // Commit: kirim command batch ke DWM
        hr = m_dcompDevice->Commit();
        if (FAILED(hr)) { OutputDebugStringA("[DComp] Commit FAILED\n"); return; }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CreateBuffers — dipakai oleh KEDUA mode (normal & transparent)
    //                 keduanya sudah punya m_swapChain valid di sini
    // ─────────────────────────────────────────────────────────────────────────
    void Window::CreateBuffers(int w, int h)
    {
        if (!m_swapChain) { OutputDebugStringA("Error: SwapChain nullptr!\n"); return; }
        ID3D11Device* device = Graphics::Instance().GetDevice();
        m_renderTargetView.Reset();
        m_depthStencilView.Reset();

        // RTV dari back buffer swap chain
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
        device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());

        // Depth-stencil
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = w;
        depthDesc.Height = h;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
        device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
        device->CreateDepthStencilView(depthTex.Get(), nullptr, m_depthStencilView.GetAddressOf());

        m_viewport.Width = (float)w;
        m_viewport.Height = (float)h;
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
        m_viewport.TopLeftX = 0;
        m_viewport.TopLeftY = 0;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BeginRender
    //
    // Untuk DComp window:
    //   - Tunggu frame latency waitable agar CPU tidak melaju terlalu jauh
    //     di depan GPU (mengurangi latency, bukan sekedar throttle)
    //   - Clear color harus PREMULTIPLIED: (r*a, g*a, b*a, a)
    //     DWM menginterpretasikan pixel sebagai premultiplied alpha.
    //
    // CATATAN SHADER: Pixel shader yang menulis ke swap chain transparent
    // juga harus output premultiplied: out.rgba = float4(rgb * a, a)
    // ─────────────────────────────────────────────────────────────────────────
    void Window::BeginRender(float r, float g, float b, float a)
    {
        // CATATAN: WaitFrameLatency() tidak dipanggil di sini.
        // Panggil dari WindowManager (sekali sebelum loop window),
        // bukan per-window — kalau per-window → stall × jumlah window = 1 FPS!
        auto context = Graphics::Instance().GetDeviceContext();

        float color[4];
        if (m_isTransparent)
        {
            // Premultiply: DWM expect format ini dari swap chain DComp
            color[0] = r * a;
            color[1] = g * a;
            color[2] = b * a;
            color[3] = a;
        }
        else
        {
            color[0] = r;
            color[1] = g;
            color[2] = b;
            color[3] = a;
        }

        context->ClearRenderTargetView(m_renderTargetView.Get(), color);
        context->ClearDepthStencilView(m_depthStencilView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(),
            m_depthStencilView.Get());
        context->RSSetViewports(1, &m_viewport);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // EndRender — unified Present untuk kedua mode
    //
    // DComp: Present() saja sudah cukup.
    //        DWM compositor yang handle compositing per-frame secara otomatis.
    //        Commit() hanya dibutuhkan kalau ada perubahan visual tree DComp
    //        (misal: transform, opacity) — di sini tidak ada, jadi tidak perlu.
    //
    // Normal: Present() + optional DXGI_PRESENT_ALLOW_TEARING kalau vsync off
    // ─────────────────────────────────────────────────────────────────────────
    void Window::EndRender(int syncInterval)
    {
        PerformanceLogger::Instance().StartTimer(PerfBucket::WindowOS);

        UINT presentFlags = 0;
        // ALLOW_TEARING hanya valid untuk normal window (bukan DComp).
        // DComp swap chain dibuat tanpa flag ALLOW_TEARING → Present akan
        // return DXGI_ERROR_INVALID_CALL kalau flag ini dipakai → handle stuck!
        if (!m_isTransparent && syncInterval == 0 && Graphics::Instance().IsTearingSupported())
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        ScheduleHitTestReadback();
        m_swapChain->Present(syncInterval, presentFlags);

        PerformanceLogger::Instance().StopTimer(PerfBucket::WindowOS);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Resize — unified untuk kedua mode
    // ─────────────────────────────────────────────────────────────────────────
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

        // ResizeBuffers bekerja sama untuk kedua jenis swap chain
        UINT resizeFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        if (!m_isTransparent && Graphics::Instance().IsTearingSupported())
            resizeFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        m_swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, resizeFlags);
        // Handle waitable lama mungkin invalid setelah resize — ambil ulang
        if (m_frameLatencyHandle)
        {
            CloseHandle(m_frameLatencyHandle);
            m_frameLatencyHandle = nullptr;
        }
        Microsoft::WRL::ComPtr<IDXGISwapChain2> sc2;
        if (SUCCEEDED(m_swapChain.As(&sc2)))
            m_frameLatencyHandle = sc2->GetFrameLatencyWaitableObject();

        CreateBuffers(w, h);

        // Tidak perlu Commit() DComp lagi — swap chain di-resize in-place,
        // visual tree tidak berubah. DComp tetap valid.
    }

    // ─────────────────────────────────────────────────────────────────────────
    // RenderBorderD3D — placeholder untuk border via GPU
    //
    // TODO: Hubungkan ke ShapeRenderer / PrimitiveRenderer yang sudah ada.
    //       Setelah BeginRender, panggil ini untuk gambar 4 rect tipis.
    //       Contoh pakai ShapeRenderer (sesuaikan dengan API kamu):
    //
    //   auto* sr = Graphics::Instance().GetShapeRenderer();
    //   float w = (float)m_width, h = (float)m_height;
    //   float bw = (float)BORDER_WIDTH;
    //   // top, bottom, left, right
    //   sr->DrawFilledRect(0, 0,   w,  bw,  1,1,0, BORDER_A/255.f); // kuning
    //   sr->DrawFilledRect(0, h-bw, w, bw,  1,1,0, BORDER_A/255.f);
    //   sr->DrawFilledRect(0, 0,   bw, h,   1,1,0, BORDER_A/255.f);
    //   sr->DrawFilledRect(w-bw,0, bw, h,   1,1,0, BORDER_A/255.f);
    // ─────────────────────────────────────────────────────────────────────────
    void Window::RenderBorderD3D()
    {
        if (!m_showBorder || !m_isTransparent) return;
        // Implementasi di atas — uncomment setelah ShapeRenderer API diketahui
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Misc setters
    // ─────────────────────────────────────────────────────────────────────────
    void Window::SetVisible(bool visible)
    {
        m_isVisible = visible;
        if (visible) SDL_ShowWindow(m_sdlWindow);
        else         SDL_HideWindow(m_sdlWindow);
    }

    void Window::SetTitle(const char* title)
    {
        if (m_sdlWindow) SDL_SetWindowTitle(m_sdlWindow, title);
    }

    void Window::SetClickThrough(bool enable)
    {
        m_isClickThrough = enable;
        if (m_hWnd)
        {
            LONG exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
            if (enable) exStyle |= WS_EX_TRANSPARENT;
            else        exStyle &= ~WS_EX_TRANSPARENT;
            SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);
        }
    }

    bool Window::ShouldRender(float dt)
    {
        if (m_frameInterval <= 0.0f) return true;
        m_accumulatedTime += dt;
        if (m_accumulatedTime >= m_frameInterval)
        {
            m_accumulatedTime = 0.0f;
            return true;
        }
        return false;
    }

    void Window::ScheduleHitTestReadback()
    {
        if (!m_isTransparent || !m_swapChain) return;

        auto device = Graphics::Instance().GetDevice();
        auto context = Graphics::Instance().GetDeviceContext();

        // Buat/recreate staging texture kalau ukuran berubah
        if (!m_hitTestStaging || m_stagingW != m_width || m_stagingH != m_height)
        {
            m_hitTestStaging.Reset();
            m_stagingReady = false;
            m_stagingW = m_width;
            m_stagingH = m_height;

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = m_width;
            desc.Height = m_height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, m_hitTestStaging.GetAddressOf())))
                return;
        }

        // Async copy dari back buffer ke staging (tidak stall GPU)
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (SUCCEEDED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        {
            context->CopyResource(m_hitTestStaging.Get(), backBuffer.Get());
            m_stagingReady = true;
        }
    }

    float Window::SampleAlphaAt(int screenX, int screenY)
    {
        if (!m_stagingReady || !m_hitTestStaging) return 1.0f; // default: solid

        RECT rc; GetWindowRect(m_hWnd, &rc);
        int localX = screenX - rc.left;
        int localY = screenY - rc.top;

        if (localX < 0 || localY < 0 || localX >= m_width || localY >= m_height)
            return 0.0f;

        auto context = Graphics::Instance().GetDeviceContext();
        D3D11_MAPPED_SUBRESOURCE mapped = {};

        // Map hanya region kecil — tapi D3D11 staging harus full map
        if (FAILED(context->Map(m_hitTestStaging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            return 1.0f;

        // Format BGRA: setiap pixel 4 byte, alpha di byte ke-3
        const uint8_t* row = (const uint8_t*)mapped.pData + localY * mapped.RowPitch;
        float alpha = row[localX * 4 + 3] / 255.0f;  // index 3 = Alpha di BGRA

        context->Unmap(m_hitTestStaging.Get(), 0);
        return alpha;
    }

    void Window::SetAlwaysOnTop(bool isTop)
    {
        if (m_sdlWindow) {
            SDL_SetWindowAlwaysOnTop(m_sdlWindow, isTop);
        }
    }
}