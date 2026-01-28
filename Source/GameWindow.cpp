#include "GameWindow.h"
#include "System/Graphics.h"
#include "Framework.h" 
#include "WindowManager.h" // [WAJIB] Tambahkan ini untuk panggil HandleResize
#include <map>
#include <iostream> 

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static std::map<HWND, WNDPROC> g_WindowProcMap;
static const UINT_PTR IDT_RESIZE_TIMER = 101;

LRESULT CALLBACK UnifiedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    // 1. Ambil pointer GameWindow dari HWND
    GameWindow* pWindow = (GameWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    // 2. Cegat Perintah Move (Drag Title Bar)
    if (msg == WM_SYSCOMMAND)
    {
        // Cek apakah perintahnya adalah SC_MOVE (sedang mau didrag)
        if ((wParam & 0xFFF0) == SC_MOVE)
        {
            // Kalau window ini diset TIDAK BOLEH DRAG, kita return 0 (batalkan)
            if (pWindow && !pWindow->IsDraggable())
            {
                return 0;
            }
        }
    }

    // 1. ImGui Priority
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
            if (Framework::Instance()) Framework::Instance()->ForceUpdateRender();
            return 0;
        }
        break;

        // -------------------------------------------------------------
        // [FIX STRETCHING] TANGKAP EVENT RESIZE DISINI!
        // -------------------------------------------------------------
    case WM_SIZE:
        // Jangan proses jika window sedang diminimize
        if (wParam != SIZE_MINIMIZED)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Panggil fungsi resize di Manager agar resolusi buffer diperbarui
            // (Pastikan kamu sudah include "WindowManager.h" di atas)
            WindowManager::Instance().HandleResize(hWnd, width, height);
        }
        // Jangan return 0 disini, biarkan pesan lanjut ke DefWindowProc/SDL
        // agar window system tetap tahu ukurannya berubah.
        break;
    }

    // 3. Chain Call
    if (g_WindowProcMap.find(hWnd) != g_WindowProcMap.end())
    {
        return CallWindowProc(g_WindowProcMap[hWnd], hWnd, msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ... (Sisa fungsi Constructor, Destructor, CreateBuffers, dll TETAP SAMA) ...
// Pastikan bagian bawah file ini tidak hilang/terhapus saat copy-paste.
// Gunakan isi file GameWindow.cpp sebelumnya untuk bagian bawahnya.

GameWindow::GameWindow(const char* title, int w, int h)
    : width(w), height(h)
{
    // ... (Kode Constructor SAMA) ...
    // Copy dari file sebelumnya
    std::cout << "[GameWindow] Creating Window: " << title << "..." << std::endl;
    sdlWindow = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE);
    hWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
    
    Graphics::Instance().CreateSwapChain(hWnd, width, height, swapChain.GetAddressOf());
    CreateBuffers(width, height);
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)UnifiedWindowProc);
    if (oldProc) g_WindowProcMap[hWnd] = oldProc;
}

GameWindow::~GameWindow()
{
    // ... (Kode Destructor SAMA) ...
    if (hWnd) g_WindowProcMap.erase(hWnd);
    if (sdlWindow) SDL_DestroyWindow(sdlWindow);
}

void GameWindow::CreateBuffers(int w, int h)
{
    // ... (Kode CreateBuffers SAMA) ...
    // Pastikan kode ini ada, karena ini yang dipanggil saat resize!
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
    // ... (Kode BeginRender SAMA) ...
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

    // Update data ukuran
    width = w; height = h;

    // Reset buffer lama
    auto context = Graphics::Instance().GetDeviceContext();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    renderTargetView.Reset();
    depthStencilView.Reset();
    context->Flush();

    // Ubah ukuran buffer SwapChain
    swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    // Buat ulang View Render & Depth baru dengan ukuran baru
    CreateBuffers(width, height);
}

void GameWindow::SetVisible(bool visible)
{
    isVisible = visible;
    if (visible) SDL_ShowWindow(sdlWindow);
    else SDL_HideWindow(sdlWindow);
}