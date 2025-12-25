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


struct CSConstants
{
    float Resolution[2];
    float Time;
    float Progress;
};


class Renderer
{
    private:
        uint16_t m_Width = 0;
        uint16_t m_Height = 0;
        bool m_COMInitialized = false;

        Microsoft::WRL::ComPtr<ID3D11Device> m_pD3DDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pD3DContext;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pRenderTex;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pRenderUAV;

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCS;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCSConstants;

        Microsoft::WRL::ComPtr<ID2D1Factory1> m_pD2DFactory;
        Microsoft::WRL::ComPtr<ID2D1Device> m_pD2DDevice;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_pD2DContext;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_pD2DTargetBitmap;

        Microsoft::WRL::ComPtr<IDWriteFactory> m_pDWriteFactory;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_pCurrentTextFormat;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pBackgroundTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pBackgroundSRV;

        std::wstring m_CurrentFontFamily;
        float m_CurrentFontSize = 24.0f;
        DWRITE_FONT_WEIGHT m_CurrentFontWeight = DWRITE_FONT_WEIGHT_NORMAL;


    public:
        Renderer(const uint16_t& width, const uint16_t& height);
        ~Renderer();
    
        bool Initialize();

        void RenderCompute(const float& time, const float& progress01);
    
        void BeginFrame();
        void EndFrame();
    
        void DrawText
        (
            const std::wstring& text,
            const D2D1_RECT_F& rect,
            const D2D1::ColorF& color,
            const std::wstring& fontFamily = L"Consolas ligaturized v3",
            const float& fontSize = 24.0f,
            const DWRITE_FONT_WEIGHT& weight = DWRITE_FONT_WEIGHT_NORMAL
        );
    
        ID3D11Texture2D* GetRenderTexture()     { return m_pRenderTex.Get(); }
        ID3D11Device* GetDevice()               { return m_pD3DDevice.Get(); }
        ID3D11DeviceContext* GetContext()       { return m_pD3DContext.Get(); }
    
        uint16_t GetWidth()     const { return m_Width; }
        uint16_t GetHeight()    const { return m_Height; }
    

    private:
        bool CreateDevices();
        bool CreateRenderTargets();
        bool CreateD2DTargets();
        bool CreateComputePipeline();
        bool LoadBackgroundTexture();
        void CreateTextFormat(const std::wstring& fontFamily, const float& fontSize,
            const DWRITE_FONT_WEIGHT& weight);
};
