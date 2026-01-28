#include "CursorBlock.h"
#include "System/Mouse.h" // Include full definition di sini
#include <cmath>

CursorBlock::CursorBlock(ID3D11Device* device)
{
    // Setup Blend State: Invert Color Logic
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    device->CreateBlendState(&blendDesc, invertBlendState.GetAddressOf());
}

CursorBlock::~CursorBlock() {}

void CursorBlock::Initialize(float w, float h)
{
    this->width = w;
    this->height = h;
}

void CursorBlock::SetGridSnap(bool enable, float gw, float gh)
{
    useGridSnap = enable;
    gridW = gw;
    gridH = gh;
}

void CursorBlock::SetBlink(bool enable, float visibleTime, float invisibleTime)
{
    useBlink = enable;
    blinkVisibleTime = visibleTime;
    blinkInvisibleTime = invisibleTime;

    // Reset state agar saat diaktifkan, kursor mulai dari kondisi terlihat (fresh start)
    isVisible = true;
    blinkTimer = 0.0f;
}

// [UPDATED] Menerima Mouse&
void CursorBlock::Update(float dt, Mouse& mouse)
{
    // Akses data menggunakan titik (.) karena ini Reference
    float rawX = static_cast<float>(mouse.GetPositionX());
    float rawY = static_cast<float>(mouse.GetPositionY());

    // Logic Snapping
    if (useGridSnap)
    {
        posX = floorf(rawX / gridW) * gridW;
        posY = floorf(rawY / gridH) * gridH;
    }
    else
    {
        posX = rawX;
        posY = rawY;
    }

    // Logic Blinking
    if (useBlink)
    {
        blinkTimer += dt;

        float currentLimit = isVisible ? blinkVisibleTime : blinkInvisibleTime;

        if (blinkTimer >= currentLimit)
        {
            blinkTimer -= currentLimit; // Reset timer (sisa waktu diteruskan agar akurat)
            isVisible = !isVisible;     // Balik status (Muncul <-> Hilang)
        }
    }
    else
    {
        isVisible = true;
    }
}

void CursorBlock::Render(ID3D11DeviceContext* context, Primitive* primitiveBatcher)
{
    if (!isVisible || !primitiveBatcher) return;

    // 1. Simpan State Lama
    float blendFactor[4];
    UINT sampleMask;
    context->OMGetBlendState(oldBlendState.GetAddressOf(), blendFactor, &sampleMask);

    // 2. Pasang Invert Blend State
    context->OMSetBlendState(invertBlendState.Get(), nullptr, 0xFFFFFFFF);

    // 3. Gambar Kotak (Flush via Primitive)
    // Warna Amber: 0.96, 0.80, 0.23
    primitiveBatcher->Rect(posX, posY, width, height, 0.0f, 0.0f, 0.0f, 0.96f, 0.80f, 0.23f, 1.0f);
    primitiveBatcher->Render(context);

    // 4. Kembalikan State Lama
    context->OMSetBlendState(oldBlendState.Get(), blendFactor, sampleMask);
    oldBlendState.Reset();
}