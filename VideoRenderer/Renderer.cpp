#include "Renderer.h"

#include <d2d1_1helper.h>

static void PrintHR(const char* what, HRESULT hr) {
    std::cerr << what << " failed: 0x" << std::hex << hr << std::dec << "\n";
}

Renderer::Renderer(UINT width, UINT height)
    : m_width(width), m_height(height) {
}

Renderer::~Renderer() {
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

bool Renderer::Initialize() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        m_comInitialized = true;
    }
    else if (hr == RPC_E_CHANGED_MODE) {
        m_comInitialized = false;
    }
    else {
        PrintHR("CoInitializeEx", hr);
        return false;
    }

    if (!CreateDevices()) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateD2DTargets()) return false;
    if (!CreateComputePipeline()) return false;

    m_cpuPackedRGBAF16.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 8);

    std::cout << "Renderer initialized (Compute -> D2D overlay -> Readback)\n";
    return true;
}

bool Renderer::CreateDevices() {
    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL created{};
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        levels,
        _countof(levels),
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &created,
        &m_d3dContext
    );
    if (FAILED(hr)) {
        PrintHR("D3D11CreateDevice", hr);
        return false;
    }

    // D2D factory + DWrite factory
    D2D1_FACTORY_OPTIONS fo{};
#if defined(_DEBUG)
    fo.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &fo,
        reinterpret_cast<void**>(m_d2dFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) {
        PrintHR("D2D1CreateFactory(ID2D1Factory1)", hr);
        return false;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) {
        PrintHR("DWriteCreateFactory", hr);
        return false;
    }

    // Create D2D device/context from the D3D device
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        PrintHR("Query IDXGIDevice", hr);
        return false;
    }

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr)) {
        PrintHR("ID2D1Factory1::CreateDevice", hr);
        return false;
    }

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    if (FAILED(hr)) {
        PrintHR("ID2D1Device::CreateDeviceContext", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateRenderTargets() {
    // Render texture: must support UAV writes + be shareable via IDXGISurface for D2D
    D3D11_TEXTURE2D_DESC tex{};
    tex.Width = m_width;
    tex.Height = m_height;
    tex.MipLevels = 1;
    tex.ArraySize = 1;
    tex.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex.SampleDesc.Count = 1;
    tex.SampleDesc.Quality = 0; // required for UAV
    tex.Usage = D3D11_USAGE_DEFAULT;
    tex.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex.CPUAccessFlags = 0;
    tex.MiscFlags = 0;

    HRESULT hr = m_d3dDevice->CreateTexture2D(&tex, nullptr, &m_renderTex);
    if (FAILED(hr)) {
        PrintHR("CreateTexture2D(renderTex)", hr);
        return false;
    }

    // UAV for compute shader output
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = tex.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    hr = m_d3dDevice->CreateUnorderedAccessView(m_renderTex.Get(), &uavDesc, &m_renderUAV);
    if (FAILED(hr)) {
        PrintHR("CreateUnorderedAccessView", hr);
        return false;
    }

    // Staging texture for CPU readback
    D3D11_TEXTURE2D_DESC st = tex;
    st.Usage = D3D11_USAGE_STAGING;
    st.BindFlags = 0;
    st.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_d3dDevice->CreateTexture2D(&st, nullptr, &m_stagingTex);
    if (FAILED(hr)) {
        PrintHR("CreateTexture2D(stagingTex)", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateD2DTargets() {
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = m_renderTex.As(&surface);
    if (FAILED(hr)) {
        PrintHR("Query IDXGISurface", hr);
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX = 96.0f;
    bp.dpiY = 96.0f;
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &m_d2dTargetBitmap);
    if (FAILED(hr)) {
        PrintHR("CreateBitmapFromDxgiSurface", hr);
        return false;
    }

    m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
    return true;
}

bool Renderer::CreateComputePipeline() {
    // Compile ShapeCS.hlsl from disk
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3DCompileFromFile(
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

    if (FAILED(hr)) {
        if (errBlob) {
            std::cerr << "Compute shader compile error:\n"
                << static_cast<const char*>(errBlob->GetBufferPointer()) << "\n";
        }
        PrintHR("D3DCompileFromFile(ShapeCS.hlsl)", hr);
        return false;
    }

    hr = m_d3dDevice->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &m_cs);
    if (FAILED(hr)) {
        PrintHR("CreateComputeShader", hr);
        return false;
    }

    // Constant buffer
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(CSConstants);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_d3dDevice->CreateBuffer(&bd, nullptr, &m_csConstants);
    if (FAILED(hr)) {
        PrintHR("CreateBuffer(CSConstants)", hr);
        return false;
    }

    return true;
}

void Renderer::RenderCompute(float timeSeconds, float progress01) {
    // Update constants
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_d3dContext->Map(m_csConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        CSConstants* c = reinterpret_cast<CSConstants*>(mapped.pData);
        c->Resolution[0] = static_cast<float>(m_width);
        c->Resolution[1] = static_cast<float>(m_height);
        c->Time = timeSeconds;
        c->Progress = progress01;
        m_d3dContext->Unmap(m_csConstants.Get(), 0);
    }

    // Bind pipeline
    m_d3dContext->CSSetShader(m_cs.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_csConstants.Get() };
    m_d3dContext->CSSetConstantBuffers(0, 1, cbs);

    ID3D11UnorderedAccessView* uavs[] = { m_renderUAV.Get() };
    UINT initialCounts[] = { 0 };
    m_d3dContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    // Dispatch
    UINT gx = (m_width + 15) / 16;
    UINT gy = (m_height + 15) / 16;
    m_d3dContext->Dispatch(gx, gy, 1);

    // Unbind UAV to avoid hazards with D2D reading/writing same resource
    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    m_d3dContext->CSSetUnorderedAccessViews(0, 1, nullUAV, initialCounts);
    m_d3dContext->CSSetShader(nullptr, nullptr, 0);

    // Simple ordering guarantee for this sample
    m_d3dContext->Flush();
}

void Renderer::BeginFrame() {
    m_d2dContext->BeginDraw();
}

void Renderer::EndFrame() {
    HRESULT hr = m_d2dContext->EndDraw();
    if (FAILED(hr)) {
        PrintHR("D2D EndDraw", hr);
    }
}

void Renderer::CreateTextFormat(const std::wstring& fontFamily, float fontSize, DWRITE_FONT_WEIGHT weight) {
    if (m_currentTextFormat &&
        m_currentFontFamily == fontFamily &&
        m_currentFontSize == fontSize &&
        m_currentFontWeight == weight) {
        return;
    }

    m_currentTextFormat.Reset();

    Microsoft::WRL::ComPtr<IDWriteTextFormat> tf;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontFamily.c_str(),
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        &tf
    );
    if (FAILED(hr)) {
        PrintHR("CreateTextFormat", hr);
        return;
    }

    tf->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    m_currentTextFormat = tf;
    m_currentFontFamily = fontFamily;
    m_currentFontSize = fontSize;
    m_currentFontWeight = weight;
}

void Renderer::DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
    const D2D1::ColorF& color, const std::wstring& fontFamily,
    float fontSize, DWRITE_FONT_WEIGHT weight) {
    CreateTextFormat(fontFamily, fontSize, weight);
    if (!m_currentTextFormat) return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = m_d2dContext->CreateSolidColorBrush(color, &brush);
    if (FAILED(hr)) {
        PrintHR("CreateSolidColorBrush", hr);
        return;
    }

    m_d2dContext->DrawText(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        m_currentTextFormat.Get(),
        rect,
        brush.Get()
    );
}

bool Renderer::GetPixelData(BYTE** ppData, UINT* pStrideBytes) {
    if (!ppData || !pStrideBytes) return false;

    // Copy GPU -> staging
    m_d3dContext->CopyResource(m_stagingTex.Get(), m_renderTex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_d3dContext->Map(m_stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        PrintHR("Map(stagingTex)", hr);
        return false;
    }

    const UINT tightStride = m_width * 8; // RGBA16F = 8 bytes/pixel
    const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped.pData);

    for (UINT y = 0; y < m_height; ++y) {
        const uint8_t* srcRow = src + static_cast<size_t>(mapped.RowPitch) * y;
        uint8_t* dstRow = m_cpuPackedRGBAF16.data() + static_cast<size_t>(tightStride) * y;
        memcpy(dstRow, srcRow, tightStride);
    }

    m_d3dContext->Unmap(m_stagingTex.Get(), 0);

    *ppData = m_cpuPackedRGBAF16.data();
    *pStrideBytes = tightStride;
    return true;
}

void Renderer::ReleasePixelData() {
    // no-op
}
