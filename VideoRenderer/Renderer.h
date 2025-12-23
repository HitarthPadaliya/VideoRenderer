#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class Renderer {
public:
    Renderer(UINT width, UINT height);
    ~Renderer();

    bool Initialize();

    // Compute pass
    void RenderCompute(float timeSeconds, float progress01);

    // Direct2D overlay pass
    void BeginFrame();
    void EndFrame();

    void DrawText(const std::wstring& text,
        const D2D1_RECT_F& rect,
        const D2D1::ColorF& color,
        const std::wstring& fontFamily = L"Cascadia Code",
        float fontSize = 24.0f,
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);

    // Accessors for GPU-to-GPU encoding
    ID3D11Texture2D* GetRenderTexture() { return m_renderTex.Get(); }
    ID3D11Device* GetDevice() { return m_d3dDevice.Get(); }
    ID3D11DeviceContext* GetContext() { return m_d3dContext.Get(); }

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    bool CreateDevices();
    bool CreateRenderTargets();
    bool CreateD2DTargets();
    bool CreateComputePipeline();
    void CreateTextFormat(const std::wstring& fontFamily, float fontSize, DWRITE_FONT_WEIGHT weight);

private:
    struct CSConstants {
        float Resolution[2];
        float Time;
        float Progress;
    };

private:
    UINT m_width = 0;
    UINT m_height = 0;
    bool m_comInitialized = false;

    // D3D11
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_renderTex; // RGBA16F
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_renderUAV;

    // Compute pipeline
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_cs;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_csConstants;

    // D2D / DWrite
    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dTargetBitmap;

    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_currentTextFormat;

    std::wstring m_currentFontFamily;
    float m_currentFontSize = 24.0f;
    DWRITE_FONT_WEIGHT m_currentFontWeight = DWRITE_FONT_WEIGHT_NORMAL;
};
