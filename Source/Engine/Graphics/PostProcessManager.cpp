#include "PostProcessManager.h"
#include "System/Graphics.h"
#include "Framework.h" // Untuk akses Window info jika perlu

PostProcessManager::PostProcessManager()
{
    // Ambil device dari Singleton Graphics
    auto device = Graphics::Instance().GetDevice();

    // Inisialisasi Shader
    m_uberShader = std::make_unique<UberShader>(device);

    // Set default data (bisa disesuaikan)
    m_data.enabled = true;
}

void PostProcessManager::Initialize(int screenWidth, int screenHeight)
{
    CreateBuffers(screenWidth, screenHeight);
}

void PostProcessManager::OnResize(int width, int height)
{
    // Recreate buffer saat resize agar resolusi tekstur pas dengan layar
    CreateBuffers(width, height);
}

void PostProcessManager::CreateBuffers(int width, int height)
{
    auto device = Graphics::Instance().GetDevice();

    // Reset Pointer lama sebelum membuat baru
    m_renderTargetTexture.Reset();
    m_renderTargetView.Reset();
    m_shaderResourceView.Reset();
    m_depthStencilTexture.Reset();
    m_depthStencilView.Reset();

    // 1. Create Texture & RTV (Color Buffer)
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr;
    hr = device->CreateTexture2D(&textureDesc, nullptr, m_renderTargetTexture.GetAddressOf());
    if (FAILED(hr)) return; // Handle error properly in prod

    hr = device->CreateRenderTargetView(m_renderTargetTexture.Get(), nullptr, m_renderTargetView.GetAddressOf());
    if (FAILED(hr)) return;

    hr = device->CreateShaderResourceView(m_renderTargetTexture.Get(), nullptr, m_shaderResourceView.GetAddressOf());
    if (FAILED(hr)) return;

    // 2. Create Depth Buffer (Penting agar objek 3D di scene bisa di-sort depth-nya)
    textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&textureDesc, nullptr, m_depthStencilTexture.GetAddressOf());
    if (FAILED(hr)) return;

    hr = device->CreateDepthStencilView(m_depthStencilTexture.Get(), nullptr, m_depthStencilView.GetAddressOf());
    if (FAILED(hr)) return;
}

void PostProcessManager::BeginCapture()
{
    auto dc = Graphics::Instance().GetDeviceContext();

    // 1. Simpan Render Target asli (BackBuffer)
    // Kita simpan raw pointer sementara. 
    // CATATAN: Pastikan EndCapture dipanggil di frame yang sama!
    m_originalRTV = nullptr;
    m_originalDSV = nullptr;
    dc->OMGetRenderTargets(1, &m_originalRTV, &m_originalDSV);

    // 2. Switch ke Render Target milik kita (Off-screen)
    ID3D11RenderTargetView* rtv = m_renderTargetView.Get();
    ID3D11DepthStencilView* dsv = m_depthStencilView.Get();
    dc->OMSetRenderTargets(1, &rtv, dsv);

    // 3. Clear Screen (Layar Virtual kita)
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Hitam Transparan
    dc->ClearRenderTargetView(rtv, clearColor);
    dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void PostProcessManager::EndCapture(float dt)
{
    auto dc = Graphics::Instance().GetDeviceContext();

    // 1. Kembalikan Render Target ke BackBuffer asli
    dc->OMSetRenderTargets(1, &m_originalRTV, m_originalDSV);

    // Jangan lupa release ref count dari GetRenderTargets (COM Rules)
    if (m_originalRTV) { m_originalRTV->Release(); m_originalRTV = nullptr; }
    if (m_originalDSV) { m_originalDSV->Release(); m_originalDSV = nullptr; }

    // Jika fitur dimatikan, kita tidak menggambar apa-apa (Layar jadi hitam)
    // ATAU: Kita harus menggambar tekstur mentah tanpa shader. 
    // Untuk safety, kita asumsi selalu draw lewat UberShader tapi dengan param 'enabled=false' kalau perlu.
    if (!m_isEnabled) return;

    // 2. Update Waktu Global
    m_globalTime += dt;
    // Reset loop agar presisi float terjaga
    if (m_globalTime > 1000.0f) m_globalTime -= 1000.0f;

    // 3. Setup Render State untuk Full Screen Quad
    // Kita butuh state yang bersih (No Depth Test, No Culling, Opaque)
    auto rs = Graphics::Instance().GetRenderState();
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // 4. Update Parameter Shader
    m_data.time = m_globalTime;

    // 5. Draw
    m_uberShader->Draw(dc, m_shaderResourceView.Get(), m_data);

    // 6. Cleanup (Unbind SRV agar bisa ditulis lagi di frame depan)
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    dc->PSSetShaderResources(0, 1, nullSRV);
}