#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "UberShader.h"

// Kelas ini menangani "Layar Virtual" (Render Target) dan efek post-processing.
// Tujuannya agar Scene tidak perlu mengurus texture creation & shader state manual.
class PostProcessManager
{
public:
    PostProcessManager();
    ~PostProcessManager() = default;

    // Inisialisasi resource sesuai ukuran layar awal
    void Initialize(int screenWidth, int screenHeight);

    // Panggil saat window di-resize
    void OnResize(int width, int height);

    // [STEP 1] Mulai merekam apa yang dirender scene ke dalam tekstur internal
    // Otomatis membersihkan layar (Clear) dengan warna hitam.
    void BeginCapture();

    // [STEP 2] Selesai merekam, dan gambar hasilnya ke Layar Utama (BackBuffer)
    // dt diperlukan untuk animasi efek (seperti scanline movement)
    void EndCapture(float dt);

    // Akses ke parameter shader untuk dimodifikasi lewat GUI/Logika Game
    UberShader::UberData& GetData() { return m_data; }

    // Toggle switch utama (jika false, EndCapture tidak akan menggambar efek, tapi pass-through atau skip)
    bool IsEnabled() const { return m_isEnabled; }
    void SetEnabled(bool enable) { m_isEnabled = enable; }

private:
    // --- Resources ---
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

    // Depth buffer khusus untuk render target ini
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_depthStencilView;

    // Simpan pointer ke Render Target Asli (BackBuffer) sebelum kita switch
    ID3D11RenderTargetView* m_originalRTV = nullptr;
    ID3D11DepthStencilView* m_originalDSV = nullptr;

    // --- Shader System ---
    std::unique_ptr<UberShader> m_uberShader;
    UberShader::UberData        m_data;

    // State
    bool m_isEnabled = true;
    float m_globalTime = 0.0f;

    // Helper untuk membuat buffer DirectX
    void CreateBuffers(int width, int height);
};