#pragma once

#include <SDL3/SDL.h>
#include <d3d11.h>
#include <wrl.h>
#include "Camera.h"

class GameWindow
{
public:
    // Create new window and initialize DirectX resources for it
    GameWindow(const char* title, int width, int height);
    ~GameWindow();

    // The Camera this window will render
    void SetCamera(Camera* cam) { this->targetCamera = cam; }
    Camera* GetCamera() const { return targetCamera; }

    // Window Management
    void BeginRender(float r = 0.0f, float g = 0.0f, float b = 0.0f); // Clear & Set Target
    void EndRender(int syncInterval = 1);
    void Resize(int width, int height);

    // Getters
    SDL_Window* GetSDLWindow() { return sdlWindow; }
    HWND GetHWND() { return hWnd; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    void SetPriority(int p) { priority = p; }
    int GetPriority() const { return priority; }

    void SetVisible(bool visible);
    bool IsVisible() const { return isVisible; }

    void SetDraggable(bool enable) { isDraggable = enable; }
    bool IsDraggable() const { return isDraggable; }

    void* GetHandle() const { return m_hWnd; }

    void SetTargetFPS(float fps)
    {
        if (fps > 0.0f) m_renderInterval = 1.0f / fps;
        else m_renderInterval = 0.0f;
    }

    // [BARU] Cek apakah window boleh dirender frame ini?
    bool ShouldRender(float dt)
    {
        if (m_renderInterval <= 0.0f) return true; // Unlimited

        m_renderTimer += dt;
        if (m_renderTimer >= m_renderInterval)
        {
            m_renderTimer = 0.0f; // Reset
            return true;
        }
        return false;
    }

private:
    void CreateBuffers(int w, int h);

private:
    SDL_Window* sdlWindow = nullptr;
    HWND hWnd = nullptr;
    int width = 0;
    int height = 0;

    Camera* targetCamera = nullptr;

    // Window specific resources
    Microsoft::WRL::ComPtr<IDXGISwapChain>          swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;
    D3D11_VIEWPORT                                  viewport = {};

    int priority = 100;

    bool isVisible = true;

    bool isDraggable = true;

    HWND m_hWnd;

    float m_renderInterval = 0.0f; // 0 = Render setiap frame
    float m_renderTimer = 0.0f;
};