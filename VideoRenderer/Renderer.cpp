#include "Renderer.h"
#include <iostream>

Renderer::Renderer(UINT width, UINT height)
    : m_width(width)
    , m_height(height)
    , m_pD2DFactory(nullptr)
    , m_pDWriteFactory(nullptr)
    , m_pWICFactory(nullptr)
    , m_pWICBitmap(nullptr)
    , m_pRenderTarget(nullptr)
    , m_pCurrentBrush(nullptr)
    , m_pCurrentTextFormat(nullptr)
    , m_pBitmapLock(nullptr)
    , m_currentBrushColor(D2D1::ColorF::Black)
    , m_currentFontSize(24.0f)
    , m_currentFontWeight(DWRITE_FONT_WEIGHT_NORMAL)
{
}

Renderer::~Renderer() {
    if (m_pBitmapLock) m_pBitmapLock->Release();
    if (m_pCurrentTextFormat) m_pCurrentTextFormat->Release();
    if (m_pCurrentBrush) m_pCurrentBrush->Release();
    if (m_pRenderTarget) m_pRenderTarget->Release();
    if (m_pWICBitmap) m_pWICBitmap->Release();
    if (m_pWICFactory) m_pWICFactory->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pD2DFactory) m_pD2DFactory->Release();
}

bool Renderer::Initialize() {
    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::cerr << "Failed to initialize COM: 0x" << std::hex << hr << std::hex << std::endl;
        return false;
    }

    // Create Direct2D factory
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) {
        std::cerr << "Failed to create Direct2D factory: 0x" << std::hex << hr << std::hex << std::endl;
        return false;
    }

    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create DirectWrite factory" << std::endl;
        return false;
    }

    // Create WIC factory
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_pWICFactory)
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create WIC factory" << std::endl;
        return false;
    }

    // Create WIC bitmap
    hr = m_pWICFactory->CreateBitmap(
        m_width,
        m_height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnDemand,
        &m_pWICBitmap
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create WIC bitmap" << std::endl;
        return false;
    }

    // Create render target from WIC bitmap
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.pixelFormat = D2D1::PixelFormat(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED
    );

    hr = m_pD2DFactory->CreateWicBitmapRenderTarget(
        m_pWICBitmap,
        props,
        &m_pRenderTarget
    );
    if (FAILED(hr)) {
        std::cerr << "Failed to create render target" << std::endl;
        return false;
    }

    // Create default brush
    CreateBrush(D2D1::ColorF::Black);

    std::cout << "Renderer initialized successfully (" << m_width << "x" << m_height << ")" << std::endl;
    return true;
}

void Renderer::BeginFrame() {
    m_pRenderTarget->BeginDraw();
}

void Renderer::EndFrame() {
    HRESULT hr = m_pRenderTarget->EndDraw();
    if (FAILED(hr)) {
        std::cerr << "EndDraw failed: 0x" << std::hex << hr << std::endl;
    }
}

void Renderer::Clear(D2D1::ColorF color) {
    m_pRenderTarget->Clear(color);
}

void Renderer::CreateBrush(const D2D1::ColorF& color) {
    if (m_pCurrentBrush && m_currentBrushColor.r == color.r &&
        m_currentBrushColor.g == color.g && m_currentBrushColor.b == color.b &&
        m_currentBrushColor.a == color.a) {
        return; // Reuse existing brush
    }

    if (m_pCurrentBrush) {
        m_pCurrentBrush->Release();
    }

    m_pRenderTarget->CreateSolidColorBrush(color, &m_pCurrentBrush);
    m_currentBrushColor = color;
}

void Renderer::DrawRectangle(const D2D1_RECT_F& rect, const D2D1::ColorF& color, float strokeWidth) {
    CreateBrush(color);
    m_pRenderTarget->DrawRectangle(rect, m_pCurrentBrush, strokeWidth);
}

void Renderer::FillRectangle(const D2D1_RECT_F& rect, const D2D1::ColorF& color) {
    CreateBrush(color);
    m_pRenderTarget->FillRectangle(rect, m_pCurrentBrush);
}

void Renderer::DrawRoundedRectangle(const D2D1_RECT_F& rect, float radiusX, float radiusY,
    const D2D1::ColorF& color, float strokeWidth) {
    CreateBrush(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(rect, radiusX, radiusY);
    m_pRenderTarget->DrawRoundedRectangle(roundedRect, m_pCurrentBrush, strokeWidth);
}

void Renderer::FillRoundedRectangle(const D2D1_RECT_F& rect, float radiusX, float radiusY,
    const D2D1::ColorF& color) {
    CreateBrush(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(rect, radiusX, radiusY);
    m_pRenderTarget->FillRoundedRectangle(roundedRect, m_pCurrentBrush);
}

void Renderer::DrawEllipse(const D2D1_POINT_2F& center, float radiusX, float radiusY,
    const D2D1::ColorF& color, float strokeWidth) {
    CreateBrush(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(center, radiusX, radiusY);
    m_pRenderTarget->DrawEllipse(ellipse, m_pCurrentBrush, strokeWidth);
}

void Renderer::FillEllipse(const D2D1_POINT_2F& center, float radiusX, float radiusY,
    const D2D1::ColorF& color) {
    CreateBrush(color);
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(center, radiusX, radiusY);
    m_pRenderTarget->FillEllipse(ellipse, m_pCurrentBrush);
}

void Renderer::DrawLine(const D2D1_POINT_2F& start, const D2D1_POINT_2F& end,
    const D2D1::ColorF& color, float strokeWidth) {
    CreateBrush(color);
    m_pRenderTarget->DrawLine(start, end, m_pCurrentBrush, strokeWidth);
}

void Renderer::CreateTextFormat(const std::wstring& fontFamily, float fontSize,
    DWRITE_FONT_WEIGHT weight) {
    if (m_pCurrentTextFormat && m_currentFontFamily == fontFamily &&
        m_currentFontSize == fontSize && m_currentFontWeight == weight) {
        return; // Reuse existing format
    }

    if (m_pCurrentTextFormat) {
        m_pCurrentTextFormat->Release();
        m_pCurrentTextFormat = nullptr;
    }

    HRESULT hr = m_pDWriteFactory->CreateTextFormat(
        fontFamily.c_str(),
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        &m_pCurrentTextFormat
    );

    if (SUCCEEDED(hr)) {
        m_currentFontFamily = fontFamily;
        m_currentFontSize = fontSize;
        m_currentFontWeight = weight;
    }
}

void Renderer::DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
    const D2D1::ColorF& color, const std::wstring& fontFamily,
    float fontSize, DWRITE_FONT_WEIGHT weight) {
    CreateBrush(color);
    CreateTextFormat(fontFamily, fontSize, weight);

    if (m_pCurrentTextFormat) {
        m_pRenderTarget->DrawText(
            text.c_str(),
            static_cast<UINT32>(text.length()),
            m_pCurrentTextFormat,
            rect,
            m_pCurrentBrush
        );
    }
}

bool Renderer::GetPixelData(BYTE** ppData, UINT* pStride) {
    WICRect rect = { 0, 0, static_cast<INT>(m_width), static_cast<INT>(m_height) };

    HRESULT hr = m_pWICBitmap->Lock(&rect, WICBitmapLockRead, &m_pBitmapLock);
    if (FAILED(hr)) {
        std::cerr << "Failed to lock WIC bitmap" << std::endl;
        return false;
    }

    UINT bufferSize = 0;
    hr = m_pBitmapLock->GetDataPointer(&bufferSize, ppData);
    if (FAILED(hr)) {
        std::cerr << "Failed to get data pointer" << std::endl;
        m_pBitmapLock->Release();
        m_pBitmapLock = nullptr;
        return false;
    }

    hr = m_pBitmapLock->GetStride(pStride);
    if (FAILED(hr)) {
        std::cerr << "Failed to get stride" << std::endl;
        m_pBitmapLock->Release();
        m_pBitmapLock = nullptr;
        return false;
    }

    return true;
}

void Renderer::ReleasePixelData() {
    if (m_pBitmapLock) {
        m_pBitmapLock->Release();
        m_pBitmapLock = nullptr;
    }
}
