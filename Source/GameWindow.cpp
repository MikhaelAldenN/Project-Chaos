#include "GameWindow.h"
#include "System/Graphics.h"
#include "Framework.h" 
#include <map>
#include <iostream> // Untuk log debug

// [WAJIB] Deklarasi eksternal agar kita bisa memanggil handler ImGui
// Fungsi ini ada di library ImGui, kita cuma perlu kasih tahu compiler dia ada.
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static std::map<HWND, WNDPROC> g_WindowProcMap;
static const UINT_PTR IDT_RESIZE_TIMER = 101;

LRESULT CALLBACK UnifiedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // -------------------------------------------------------------------------
    // 1. PRIORITAS UTAMA: Berikan pesan ke ImGui Handle dulu!
    // -------------------------------------------------------------------------
    // Jika ImGui ingin memproses pesan ini (misal mouse click di UI),
    // maka biarkan dia memprosesnya dan kita return true.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // -------------------------------------------------------------------------
    // 2. Logic Game Kita (Resize, Timer, dll)
    // -------------------------------------------------------------------------
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
            if (Framework::Instance()) Framework::Instance()->ForceUpdateRender();
            return 0;
        }
        break;
    }

    // -------------------------------------------------------------------------
    // 3. Panggil Prosedur Asli (Chain Call)
    // -------------------------------------------------------------------------
    // Cek apakah HWND ada di map (artinya window ini milik game kita)
    if (g_WindowProcMap.find(hWnd) != g_WindowProcMap.end())
    {
        return CallWindowProc(g_WindowProcMap[hWnd], hWnd, msg, wParam, lParam);
    }

    // Fallback jika message datang SEBELUM map terisi (Race Condition saat Init)
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

GameWindow::GameWindow(const char* title, int w, int h)
    : width(w), height(h)
{
    std::cout << "[GameWindow] Creating Window: " << title << "..." << std::endl;

    // 1. Create SDL Window
    sdlWindow = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE);
    if (!sdlWindow) {
        std::cerr << "[GameWindow] Error: SDL_CreateWindow failed!" << std::endl;
        return;
    }

    // 2. Ambil HWND
    hWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (!hWnd) {
        std::cerr << "[GameWindow] Error: Failed to get HWND!" << std::endl;
        return;
    }

    // 3. Setup DirectX
    Graphics::Instance().CreateSwapChain(hWnd, width, height, swapChain.GetAddressOf());

    // Cek manual apakah swapChain berhasil dibuat (tidak null)
    if (!swapChain) {
        std::cerr << "[GameWindow] Error: CreateSwapChain failed (Swapchain is NULL)!" << std::endl;
        return;
    }

    CreateBuffers(width, height);

    // 4. Pasang Hook
    // Simpan prosedur lama (SDL Proc)
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)UnifiedWindowProc);

    // Simpan ke map
    if (oldProc) {
        g_WindowProcMap[hWnd] = oldProc;
        std::cout << "[GameWindow] Hook installed for HWND: " << hWnd << std::endl;
    }
    else {
        std::cerr << "[GameWindow] Warning: SetWindowLongPtr returned NULL/Error!" << std::endl;
    }
}

GameWindow::~GameWindow()
{
    std::cout << "[GameWindow] Destroying Window..." << std::endl;
    if (hWnd) g_WindowProcMap.erase(hWnd);
    if (sdlWindow) SDL_DestroyWindow(sdlWindow);
}

// ... (Sisa fungsi CreateBuffers, BeginRender, EndRender, Resize SAMA SEPERTI SEBELUMNYA) ...
// (Pastikan copy-paste bagian CreateBuffers dst dari file lamamu agar lengkap)
void GameWindow::CreateBuffers(int w, int h)
{
    ID3D11Device* device = Graphics::Instance().GetDevice();
    renderTargetView.Reset();
    depthStencilView.Reset();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = w; depthDesc.Height = h;
    depthDesc.MipLevels = 1; depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1; depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
    device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
    device->CreateDepthStencilView(depthTex.Get(), nullptr, depthStencilView.GetAddressOf());

    viewport.Width = (float)w; viewport.Height = (float)h;
    viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0; viewport.TopLeftY = 0;
}

void GameWindow::BeginRender(float r, float g, float b)
{
    auto context = Graphics::Instance().GetDeviceContext();
    float color[] = { r, g, b, 1.0f };
    context->ClearRenderTargetView(renderTargetView.Get(), color);
    context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
    context->RSSetViewports(1, &viewport);
}

void GameWindow::EndRender(int syncInterval)
{
    if (swapChain) swapChain->Present(syncInterval, 0);
}

void GameWindow::Resize(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (w == width && h == height) return;
    width = w; height = h;
    auto context = Graphics::Instance().GetDeviceContext();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    renderTargetView.Reset();
    depthStencilView.Reset();
    context->Flush();
    swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateBuffers(width, height);
}