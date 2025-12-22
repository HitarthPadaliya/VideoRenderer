#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <memory>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

class Renderer {
public:
    Renderer(UINT width, UINT height);
    ~Renderer();

    // Initialize all Direct2D resources
    bool Initialize();

    // Start/End drawing
    void BeginFrame();
    void EndFrame();

    // Clear canvas
    void Clear(D2D1::ColorF color = D2D1::ColorF::White);

    // Drawing primitives
    void DrawRectangle(const D2D1_RECT_F& rect, const D2D1::ColorF& color, float strokeWidth = 1.0f);
    void FillRectangle(const D2D1_RECT_F& rect, const D2D1::ColorF& color);
    void DrawRoundedRectangle(const D2D1_RECT_F& rect, float radiusX, float radiusY,
        const D2D1::ColorF& color, float strokeWidth = 1.0f);
    void FillRoundedRectangle(const D2D1_RECT_F& rect, float radiusX, float radiusY,
        const D2D1::ColorF& color);
    void DrawEllipse(const D2D1_POINT_2F& center, float radiusX, float radiusY,
        const D2D1::ColorF& color, float strokeWidth = 1.0f);
    void FillEllipse(const D2D1_POINT_2F& center, float radiusX, float radiusY,
        const D2D1::ColorF& color);
    void DrawLine(const D2D1_POINT_2F& start, const D2D1_POINT_2F& end,
        const D2D1::ColorF& color, float strokeWidth = 1.0f);

    // Text rendering with ligatures
    void DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
        const D2D1::ColorF& color, const std::wstring& fontFamily = L"Cascadia Code",
        float fontSize = 24.0f, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);

    // Get pixel data for encoding
    bool GetPixelData(BYTE** ppData, UINT* pStride);
    void ReleasePixelData();

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    void CreateBrush(const D2D1::ColorF& color);
    void CreateTextFormat(const std::wstring& fontFamily, float fontSize,
        DWRITE_FONT_WEIGHT weight);

    UINT m_width;
    UINT m_height;

    // COM interfaces
    ID2D1Factory* m_pD2DFactory;
    IDWriteFactory* m_pDWriteFactory;
    IWICImagingFactory* m_pWICFactory;
    IWICBitmap* m_pWICBitmap;
    ID2D1RenderTarget* m_pRenderTarget;

    // Current brush and text format (cached)
    ID2D1SolidColorBrush* m_pCurrentBrush;
    IDWriteTextFormat* m_pCurrentTextFormat;
    D2D1::ColorF m_currentBrushColor;
    std::wstring m_currentFontFamily;
    float m_currentFontSize;
    DWRITE_FONT_WEIGHT m_currentFontWeight;

    // Pixel access
    IWICBitmapLock* m_pBitmapLock;
};
