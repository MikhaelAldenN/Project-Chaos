#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "RenderState.h"
#include "PrimitiveRenderer.h"
#include "ShapeRenderer.h"
#include "ModelRenderer.h"

class Graphics
{
private:
    Graphics() = default;
    ~Graphics() = default;

public:
    static Graphics& Instance() { static Graphics i; return i; }

    void Initialize();
    void CreateSwapChain(HWND hWnd, int width, int height, IDXGISwapChain** outSwapChain);

    ID3D11Device* GetDevice() { return device.Get(); }
    ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }

    RenderState* GetRenderState() { return renderState.get(); }
    PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
    ShapeRenderer* GetShapeRenderer() const { return shapeRenderer.get(); }
    ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }
    ID3D11BlendState* GetAlphaBlendState();

private:
    Microsoft::WRL::ComPtr<ID3D11Device>        device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
    Microsoft::WRL::ComPtr<IDXGIFactory>        dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D11BlendState>    alphaBlendState;

    std::unique_ptr<RenderState>       renderState;
    std::unique_ptr<PrimitiveRenderer> primitiveRenderer;
    std::unique_ptr<ShapeRenderer>     shapeRenderer;
    std::unique_ptr<ModelRenderer>     modelRenderer;
};