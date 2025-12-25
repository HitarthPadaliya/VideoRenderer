#include "Renderer.h"

#include <combaseapi.h>


static void PrintHR(const char* what, HRESULT hr)
{
    std::cerr << what << " failed: 0x" << std::hex << hr << std::dec << "\n";
}


Renderer::Renderer(const uint16_t& width, const uint16_t& height)
    : m_Width(width)
    , m_Height(height)
{}

Renderer::~Renderer()
{
    if (m_COMInitialized)
    {
        CoUninitialize();
        m_COMInitialized = false;
    }
}


bool Renderer::Initialize()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
        m_COMInitialized = true;
    else if (hr == RPC_E_CHANGED_MODE)
        m_COMInitialized = false;
    else
    {
        PrintHR("CoInitializeEx", hr);
        return false;
    }

    if (!CreateDevices())           return false;
    if (!CreateRenderTargets())     return false;
    if (!CreateD2DTargets())        return false;
    if (!CreateComputePipeline())   return false;

    std::cout << "Renderer initialized\n";
    return true;
}

bool Renderer::CreateDevices()
{
    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL created {};
    HRESULT hr = D3D11CreateDevice
    (
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        levels,
        _countof(levels),
        D3D11_SDK_VERSION,
        &m_pD3DDevice,
        &created,
        &m_pD3DContext
    );
    if (FAILED(hr))
    {
        PrintHR("D3D11CreateDevice", hr);
        return false;
    }

    D2D1_FACTORY_OPTIONS fo{};
#if defined(_DEBUG)
    fo.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    hr = D2D1CreateFactory
    (
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &fo,
        reinterpret_cast<void**>(m_pD2DFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        PrintHR("D2D1CreateFactory(ID2D1Factory1)", hr);
        return false;
    }

    hr = DWriteCreateFactory
    (
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_pDWriteFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        PrintHR("DWriteCreateFactory", hr);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_pD3DDevice.As(&dxgiDevice);
    if (FAILED(hr))
    {
        PrintHR("Query IDXGIDevice", hr);
        return false;
    }

    hr = m_pD2DFactory->CreateDevice(dxgiDevice.Get(), &m_pD2DDevice);
    if (FAILED(hr))
    {
        PrintHR("ID2D1Factory1::CreateDevice", hr);
        return false;
    }

    hr = m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DContext);
    if (FAILED(hr))
    {
        PrintHR("ID2D1Device::CreateDeviceContext", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateRenderTargets()
{
    D3D11_TEXTURE2D_DESC tex {};
    tex.Width                   = m_Width;
    tex.Height                  = m_Height;
    tex.MipLevels               = 1;
    tex.ArraySize               = 1;
    tex.Format                  = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex.SampleDesc.Count        = 1;
    tex.SampleDesc.Quality      = 0;
    tex.Usage                   = D3D11_USAGE_DEFAULT;
    tex.BindFlags               = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex.CPUAccessFlags          = 0;
    tex.MiscFlags               = 0;

    HRESULT hr = m_pD3DDevice->CreateTexture2D(&tex, nullptr, &m_pRenderTex);
    if (FAILED(hr))
    {
        PrintHR("CreateTexture2D(renderTex)", hr);
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format              = tex.Format;
    uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice  = 0;

    hr = m_pD3DDevice->CreateUnorderedAccessView(m_pRenderTex.Get(), &uavDesc, &m_pRenderUAV);
    if (FAILED(hr))
    {
        PrintHR("CreateUnorderedAccessView(renderUAV)", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateD2DTargets()
{
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = m_pRenderTex.As(&surface);
    if (FAILED(hr))
    {
        PrintHR("Query IDXGISurface", hr);
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 bp {};
    bp.pixelFormat      = D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX             = 96.0f;
    bp.dpiY             = 96.0f;
    bp.bitmapOptions    = D2D1_BITMAP_OPTIONS_TARGET;

    hr = m_pD2DContext->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &m_pD2DTargetBitmap);
    if (FAILED(hr))
    {
        PrintHR("CreateBitmapFromDxgiSurface", hr);
        return false;
    }

    m_pD2DContext->SetTarget(m_pD2DTargetBitmap.Get());
    return true;
}

bool Renderer::CreateComputePipeline()
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3DCompileFromFile
    (
        L"ShapeCS.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CSMain",
        "cs_5_0",
        flags,
        0,
        &csBlob,
        &errBlob
    );
    if (FAILED(hr))
    {
        if (errBlob)
            std::cerr << "Compute shader compile error:\n" << (const char*)errBlob->GetBufferPointer() << "\n";
        PrintHR("D3DCompileFromFile(ShapeCS.hlsl)", hr);
        return false;
    }

    hr = m_pD3DDevice->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &m_pCS);
    if (FAILED(hr))
    {
        PrintHR("CreateComputeShader", hr);
        return false;
    }

    D3D11_BUFFER_DESC bd {};
    bd.ByteWidth        = sizeof(CSConstants);
    bd.Usage            = D3D11_USAGE_DYNAMIC;
    bd.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

    hr = m_pD3DDevice->CreateBuffer(&bd, nullptr, &m_pCSConstants);
    if (FAILED(hr))
    {
        PrintHR("CreateBuffer(CSConstants)", hr);
        return false;
    }

    return true;
}

void Renderer::RenderCompute(const float& time, const float& progress01)
{
    D3D11_MAPPED_SUBRESOURCE mapped {};
    HRESULT hr = m_pD3DContext->Map(m_pCSConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        CSConstants* c      = reinterpret_cast<CSConstants*>(mapped.pData);
        c->Resolution[0]    = static_cast<float>(m_Width);
        c->Resolution[1]    = static_cast<float>(m_Height);
        c->Time             = time;
        c->Progress         = progress01;
        c->rSize[0]         = 1920;
        c->rSize[1]         = 1080;
        m_pD3DContext->Unmap(m_pCSConstants.Get(), 0);
    }

    m_pD3DContext->CSSetShader(m_pCS.Get(), nullptr, 0);

    ID3D11Buffer* cbs[] = { m_pCSConstants.Get() };
    m_pD3DContext->CSSetConstantBuffers(0, 1, cbs);

    ID3D11UnorderedAccessView* uavs[] = { m_pRenderUAV.Get() };
    UINT initialCounts[] = { 0 };
    m_pD3DContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    UINT gx = (m_Width + 31) / 32;
    UINT gy = (m_Height + 31) / 32;
    m_pD3DContext->Dispatch(gx, gy, 1);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    m_pD3DContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, initialCounts);
    m_pD3DContext->CSSetShader(nullptr, nullptr, 0);
}

void Renderer::BeginFrame()
{
    m_pD2DContext->BeginDraw();
}

void Renderer::EndFrame()
{
    HRESULT hr = m_pD2DContext->EndDraw();
    if (FAILED(hr))
        PrintHR("D2D EndDraw", hr);
}

void Renderer::CreateTextFormat(const std::wstring& fontFamily, const float& fontSize,
    const DWRITE_FONT_WEIGHT& weight)
{
    if
    (
        m_pCurrentTextFormat &&
        m_CurrentFontFamily == fontFamily &&
        m_CurrentFontSize == fontSize &&
        m_CurrentFontWeight == weight
    )
    {
        return;
    }

    m_pCurrentTextFormat.Reset();

    Microsoft::WRL::ComPtr<IDWriteTextFormat> tf;
    HRESULT hr = m_pDWriteFactory->CreateTextFormat
    (
        fontFamily.c_str(),
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        &tf
    );
    if (FAILED(hr))
    {
        PrintHR("CreateTextFormat", hr);
        return;
    }

    tf->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    m_pCurrentTextFormat = tf;
    m_CurrentFontFamily = fontFamily;
    m_CurrentFontSize = fontSize;
    m_CurrentFontWeight = weight;
}

void Renderer::DrawText
(
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    const D2D1::ColorF& color,
    const std::wstring& fontFamily,
    const float& fontSize,
    const DWRITE_FONT_WEIGHT& weight
)
{
    CreateTextFormat(fontFamily, fontSize, weight);
    if (!m_pCurrentTextFormat)
        return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = m_pD2DContext->CreateSolidColorBrush(color, &brush);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush", hr);
        return;
    }

    m_pD2DContext->DrawText
    (
        text.c_str(),
        static_cast<UINT32>(text.size()),
        m_pCurrentTextFormat.Get(),
        rect,
        brush.Get()
    );
}
