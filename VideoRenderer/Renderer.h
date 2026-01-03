#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>
#include <vector>

#include "SyntaxHighlighter.h"
#include "Slide.h"
#include "EndInfo.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")


struct CSConstants
{
    float Resolution[2];
    float Time;
    float Scale;
    float rSize[2];
    float rSizeInitial[2];
};

struct CharState
{
    wchar_t c;
    float start;
    bool bIsWhitespace;
    bool bIsNewline;
};

struct HeaderState
{
    D2D1_POINT_2F prevPos;
    float prevScale = 1;
    float prevOpacity = 1;

    D2D1_POINT_2F pos;
    float scale = 0;
    float opacity = 0;
};


class Renderer
{
    private:
        uint16_t m_Width = 0;
        uint16_t m_Height = 0;
        bool m_COMInitialized = false;

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_pReusableBrush;

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
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pBlurredTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pBlurredSRV;

        Microsoft::WRL::ComPtr<IDWriteTextLayout> m_pHeaderLayout;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> m_pCodeLayout;

        Slide* m_pSlide;

        D2D1_POINT_2F m_HeaderPosition;
        std::wstring m_Header;
        std::wstring m_PrevHeader;
        HeaderState* m_pHeaderState = nullptr;
        std::wstring m_Code;
        float m_CodeDuration = 0.0f;
        D2D1_POINT_2F m_CodePosition;
        D2D1_POINT_2F m_CodeSize;

        SyntaxHighlighter* m_pSyntaxHighlighter;
        std::vector<Token> m_Tokens;

        std::wstring m_CurrentFontFamily;
        float m_CurrentFontSize = 72.0f;
        DWRITE_FONT_WEIGHT m_CurrentFontWeight = DWRITE_FONT_WEIGHT_NORMAL;

        std::vector<CharState> m_CharStates;
        float m_CodeAnimProgress = 0.0f;

        float m_Duration        = 0.0f;
        float m_StartScale      = 0.0f;
        float m_EndScale        = 0.0f;
        float m_CurrentScale    = 0.0f;
        D2D1_POINT_2F m_StartSize;
        D2D1_POINT_2F m_MidSize;
        D2D1_POINT_2F m_EndSize;
        D2D1_POINT_2F m_CurrentSize;
        float m_StartY  = 1080;
        float m_MidY    = 1080;
        float m_EndY    = 1080;


    public:
        Renderer(const uint16_t& width, const uint16_t& height);
        ~Renderer();
    
        bool Initialize(Slide* pSlide);
        bool InitBrushes();

        void RenderCompute(const float& time, const float& progress01);
    
        void BeginFrame();
        void EndFrame();
    
        void DrawHeader();
        void DrawCode();

        void DrawTextFromLayout
        (
            const Microsoft::WRL::ComPtr<IDWriteTextLayout>& layout,
            const D2D1::ColorF& color,
            const D2D1_POINT_2F& position
        );

        Microsoft::WRL::ComPtr<IDWriteTextLayout> GetTextMetrics
        (
            DWRITE_TEXT_METRICS* metrics,
            const std::wstring& text,
            const std::wstring& fontFamily = L"Consolas ligaturized v3",
            const float& fontSize = 72.0f,
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
        bool LoadBlurredTexture();
        void CreateTextFormat(const std::wstring& fontFamily, const float& fontSize,
            const DWRITE_FONT_WEIGHT& weight);

        void InitDecoderStates();
        void DrawTextDecoder(const D2D1::ColorF& color, float animProgress);

        static inline float LerpTime(float time, float offset, float duration);
};
