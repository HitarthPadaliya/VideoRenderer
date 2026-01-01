#include "Renderer.h"

#include "Easing.h"

#include <algorithm>
#include <combaseapi.h>
#include <WICTextureLoader.h>

static void PrintHR(const char* what, HRESULT hr)
{
    std::cerr << what << " failed: 0x" << std::hex << hr << std::dec << "\n";
}

Renderer::Renderer(const uint16_t& width, const uint16_t& height)
    : m_Width(width)
    , m_Height(height)
    , m_HeaderPosition(D2D1::Point2F(0.0f, 0.0f))
    , m_CodePosition(D2D1::Point2F(0.0f, 0.0f))
    , m_CodeSize(D2D1::Point2F(0.0f, 0.0f))
    , m_pSyntaxHighlighter(nullptr)
    , m_StartSize(D2D1::Point2F(0.0f, 0.0f))
    , m_MidSize(D2D1::Point2F(0.0f, 0.0f))
    , m_EndSize(D2D1::Point2F(0.0f, 0.0f))
    , m_CurrentSize(D2D1::Point2F(0.0f, 0.0f))
{
}

Renderer::~Renderer()
{
    delete m_pSyntaxHighlighter;
    delete m_pHeaderState;

    if (m_COMInitialized)
    {
        CoUninitialize();
        m_COMInitialized = false;
    }
}

bool Renderer::Initialize(Slide* pSlide)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        m_COMInitialized = true;
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        m_COMInitialized = false;
    }
    else
    {
        PrintHR("CoInitializeEx", hr);
        return false;
    }

    if (!CreateDevices())        return false;
    if (!CreateRenderTargets())  return false;
    if (!CreateD2DTargets())     return false;
    if (!CreateComputePipeline()) return false;
    if (!LoadBackgroundTexture()) return false;
    if (!LoadBlurredTexture())    return false;

    m_pSyntaxHighlighter = new SyntaxHighlighter(pSlide);
    if (!m_pSyntaxHighlighter)
    {
        std::cerr << "Failed to create SyntaxHighlighter\n";
        return false;
    }

    if (!InitBrushes())
        return false;

    m_Tokens = m_pSyntaxHighlighter->Tokenize();

    m_Header = pSlide->m_Header;
    m_Code = pSlide->m_Code;

    DWRITE_TEXT_METRICS mcode{};
    m_pCodeLayout = GetTextMetrics(&mcode, pSlide->m_Code);
    m_CodePosition = D2D1::Point2F(
        (3840.0f - mcode.width) * 0.5f - mcode.left,
        (2160.0f - mcode.height) * 0.5f - mcode.top + 50.0f
    );
    m_CodeSize = D2D1::Point2F(mcode.width, mcode.height);

    DWRITE_TEXT_METRICS mheader{};
    m_pHeaderLayout = GetTextMetrics(&mheader, pSlide->m_Header, L"Segoe UI", 60.0f);
    m_HeaderPosition = D2D1::Point2F(
        (3840.0f - mheader.width) * 0.5f - mheader.left,
        (2160.0f - mcode.height - mheader.height - 200.0f) * 0.5f - mheader.top + 50.0f
    );

    for (const Token& token : m_Tokens)
    {
        DWRITE_TEXT_RANGE range = { token.start, token.length };
        m_pCodeLayout->SetDrawingEffect(SyntaxHighlighter::GetBrush(token).Get(), range);
    }

    InitDecoderStates();

    m_CodeDuration = pSlide->m_CodeDuration;
    m_Duration = pSlide->m_Duration;

    if (pSlide->m_bOpenWindow || pSlide->m_SlideNo == 1)
    {
        m_StartSize = D2D1::Point2F(0.0f, 0.0f);
        m_StartScale = 0.0f;
        m_StartY = 1080.0f;
    }
    else if (pSlide->m_SlideNo > 1)
    {
        EndInfo prevEndInfo(pSlide->m_SlideNo - 1);
        m_StartSize = D2D1::Point2F(prevEndInfo.m_WindowX, prevEndInfo.m_WindowY);
        m_StartScale = 1.0f;
        m_StartY = prevEndInfo.m_HeaderY;

        Slide prevSlide(pSlide->m_SlideNo - 1);
        m_PrevHeader = prevSlide.m_Header;

        if (m_Header.compare(m_PrevHeader) != 0)
        {
            m_pHeaderState = new HeaderState();
            m_pHeaderState->prevPos = D2D1::Point2F(m_HeaderPosition.x, m_StartY);
            m_pHeaderState->pos = m_HeaderPosition;
        }
    }

    m_MidSize = D2D1::Point2F(m_CodeSize.x + 200.0f, m_CodeSize.y + 300.0f);
    m_MidY = m_HeaderPosition.y;

    if (pSlide->m_bCloseWindow)
    {
        m_EndSize = D2D1::Point2F(0.0f, 0.0f);
        m_EndScale = 0.0f;
        m_EndY = 1080.0f;
    }
    else
    {
        m_EndSize = m_MidSize;
        m_EndScale = 1.0f;
        m_EndY = m_MidY;
    }

    const float inDur = std::max(0.001f, pSlide->m_WindowInDuration);
    const float outDur = std::max(0.001f, pSlide->m_WindowOutDuration);
    const float outStart = m_Duration - outDur;

    m_TrackSizeXIn = { 0.0f, inDur,  m_StartSize.x,  m_MidSize.x,  EaseOutAEDoubleBack };
    m_TrackSizeYIn = { 0.0f, inDur,  m_StartSize.y,  m_MidSize.y,  EaseOutAEDoubleBack };
    m_TrackScaleIn = { 0.0f, inDur,  m_StartScale,   1.0f,         EaseOutAEDoubleBack };
    m_TrackHeaderYIn = { 0.0f, inDur, m_StartY,      m_MidY,       EaseOutAEDoubleBack };

    m_TrackSizeXOut = { outStart, m_Duration, m_MidSize.x,  m_EndSize.x,  EaseInAEDoubleBack };
    m_TrackSizeYOut = { outStart, m_Duration, m_MidSize.y,  m_EndSize.y,  EaseInAEDoubleBack };
    m_TrackScaleOut = { outStart, m_Duration, 1.0f,         m_EndScale,   EaseInAEDoubleBack };
    m_TrackHeaderYOut = { outStart, m_Duration, m_MidY,     m_EndY,       EaseInAEDoubleBack };

    EndInfo endinfo(pSlide->m_SlideNo, m_EndSize, m_EndY);

    std::cout << "Renderer initialized\n";
    return true;
}

bool Renderer::InitBrushes()
{
    HRESULT hr = m_pD2DContext->CreateSolidColorBrush(Colors::Function, &Brushes::Function);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Function", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Class, &Brushes::Class);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Class", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::EnumVal, &Brushes::EnumVal);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for EnumVal", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Parameter, &Brushes::Parameter);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Parameter", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::LocalVar, &Brushes::LocalVar);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for LocalVar", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::MemberVar, &Brushes::MemberVar);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for MemberVar", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Keyword, &Brushes::Keyword);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Keyword", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::ControlStatement, &Brushes::ControlStatement);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for ControlStatement", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Preprocessor, &Brushes::Preprocessor);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Preprocessor", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Comment, &Brushes::Comment);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Comment", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Macro, &Brushes::Macro);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Macro", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::UEMacro, &Brushes::UEMacro);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for UEMacro", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Number, &Brushes::Number);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Number", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::StringLiteral, &Brushes::StringLiteral);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for StringLiteral", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::CharLiteral, &Brushes::CharLiteral);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for CharLiteral", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Other, &Brushes::Other);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for Other", hr);
        return false;
    }

    hr = m_pD2DContext->CreateSolidColorBrush(Colors::Other, &m_pReusableBrush);
    if (FAILED(hr))
    {
        PrintHR("CreateSolidColorBrush for reusable brush", hr);
        return false;
    }

    return true;
}


void Renderer::RenderCompute(const float& time, const float& progress01)
{
    const float inEnd = m_TrackScaleIn.t1;
    const float outStart = m_TrackScaleOut.t0;

    if (time < inEnd)
    {
        m_CurrentSize.x = EvalTrack(m_TrackSizeXIn, time);
        m_CurrentSize.y = EvalTrack(m_TrackSizeYIn, time);
        m_CurrentScale = EvalTrack(m_TrackScaleIn, time);
        m_HeaderPosition.y = EvalTrack(m_TrackHeaderYIn, time);

        if (m_pHeaderState)
        {
            const float u = std::clamp(LerpTime(time, m_TrackScaleIn.t0, m_TrackScaleIn.t1 - m_TrackScaleIn.t0), 0.0f, 1.0f);
            const float e = EaseInOutSine(u);
            m_pHeaderState->prevScale = std::lerp(1.0f, 0.0f, e);
            m_pHeaderState->scale = std::lerp(0.0f, 1.0f, e);
            m_pHeaderState->prevOpacity = m_pHeaderState->prevScale;
            m_pHeaderState->opacity = m_pHeaderState->scale;
        }
    }
    else if (time > outStart)
    {
        m_CurrentSize.x = EvalTrack(m_TrackSizeXOut, time);
        m_CurrentSize.y = EvalTrack(m_TrackSizeYOut, time);
        m_CurrentScale = EvalTrack(m_TrackScaleOut, time);
        m_HeaderPosition.y = EvalTrack(m_TrackHeaderYOut, time);
    }
    else
    {
        m_CurrentSize = m_MidSize;
        m_CurrentScale = 1.0f;
        m_HeaderPosition.y = m_MidY;
        if (m_pHeaderState)
        {
            m_pHeaderState->prevScale = 0.0f;
            m_pHeaderState->prevOpacity = 0.0f;
            m_pHeaderState->scale = 1.0f;
            m_pHeaderState->opacity = 1.0f;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_pD3DContext->Map(m_pCSConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        CSConstants* c = reinterpret_cast<CSConstants*>(mapped.pData);
        c->Resolution[0] = static_cast<float>(m_Width);
        c->Resolution[1] = static_cast<float>(m_Height);
        c->Time = time;
        c->Scale = m_CurrentScale;
        c->rSize[0] = m_CurrentSize.x;
        c->rSize[1] = m_CurrentSize.y;
        c->rSizeInitial[0] = 0.0f;
        c->rSizeInitial[1] = 0.0f;
        m_pD3DContext->Unmap(m_pCSConstants.Get(), 0);
    }

    m_pD3DContext->CSSetShader(m_pCS.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_pCSConstants.Get() };
    m_pD3DContext->CSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* srvs[] =
    {
        m_pBackgroundSRV.Get(),
        m_pBlurredSRV.Get()
    };
    m_pD3DContext->CSSetShaderResources(0, 2, srvs);

    ID3D11UnorderedAccessView* uavs[] = { m_pRenderUAV.Get() };
    UINT initialCounts[] = { 0 };
    m_pD3DContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    const UINT gx = (m_Width + 31) / 32;
    const UINT gy = (m_Height + 31) / 32;
    m_pD3DContext->Dispatch(gx, gy, 1);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr, nullptr };
    m_pD3DContext->CSSetShaderResources(0, 2, nullSRV);
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    m_pD3DContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, initialCounts);
    m_pD3DContext->CSSetShader(nullptr, nullptr, 0);

    const float revealStart = m_TrackScaleIn.t1 + std::max(0.0f, 0.0f);
    const float revealDur = m_CodeDuration;
    const float hideStart = m_Duration - (m_Duration - m_TrackScaleOut.t0);

    if (time <= revealStart || time >= m_Duration - 0.5f)
    {
        m_CodeAnimProgress = 0.0f;
    }
    else if (time >= revealStart && time <= revealStart + revealDur)
    {
        m_CodeAnimProgress = EaseInOutSine(LerpTime(time, revealStart, revealDur));
    }
    else if (time >= m_Duration - 1.5f && time <= m_Duration - 0.5f)
    {
        m_CodeAnimProgress = 1.0f - EaseInOutSine(LerpTime(time, m_Duration - 1.5f, 1.0f));
    }
    else
    {
        m_CodeAnimProgress = 1.0f;
    }
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

void Renderer::CreateTextFormat(
    const std::wstring& fontFamily,
    const float& fontSize,
    const DWRITE_FONT_WEIGHT& weight)
{
    if (m_pCurrentTextFormat &&
        m_CurrentFontFamily == fontFamily &&
        m_CurrentFontSize == fontSize &&
        m_CurrentFontWeight == weight)
    {
        return;
    }

    m_pCurrentTextFormat.Reset();

    Microsoft::WRL::ComPtr<IDWriteTextFormat> tf;
    HRESULT hr = m_pDWriteFactory->CreateTextFormat(
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

void Renderer::DrawHeader()
{
    if (!m_pHeaderLayout)
        return;
    if (m_CurrentScale == 0.0f)
        return;

    const float stateScale = m_pHeaderState ? m_pHeaderState->scale : 1.0f;
    const float scale = stateScale * m_CurrentScale;
    if (scale <= 0.0f)
        return;

    DWRITE_TEXT_METRICS mheader{};
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout =
        GetTextMetrics(&mheader, m_Header, L"Segoe UI", 60.0f * scale);
    if (!layout)
        return;

    float yBackup = m_HeaderPosition.y;

    m_HeaderPosition.x = (3840.0f - mheader.width) * 0.5f - mheader.left;
    m_HeaderPosition.y = 1080.0f - m_CurrentSize.y * 0.5f + 100.0f * m_CurrentScale - mheader.height * 0.5f;

    const float opacity = m_pHeaderState ? m_pHeaderState->opacity : 1.0f;
    DrawTextFromLayout(layout, D2D1::ColorF(0.7059f, 0.7059f, 0.7059f, opacity), m_HeaderPosition);

    if (!m_pHeaderState || m_pHeaderState->prevScale == 0.0f)
    {
        m_HeaderPosition.y = yBackup;
        return;
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> prevLayout =
        GetTextMetrics(&mheader, m_PrevHeader, L"Segoe UI", 60.0f * m_pHeaderState->prevScale);
    if (prevLayout)
    {
        m_pHeaderState->prevPos.x =
            (3840.0f - mheader.width) * 0.5f - mheader.left;
        m_pHeaderState->prevPos.y =
            1080.0f - m_CurrentSize.y * 0.5f + 100.0f * m_CurrentScale - mheader.height * 0.5f;

        DrawTextFromLayout(
            prevLayout,
            D2D1::ColorF(0.7059f, 0.7059f, 0.7059f, m_pHeaderState->prevOpacity),
            m_pHeaderState->prevPos
        );
    }

    m_HeaderPosition.y = yBackup;
}

void Renderer::DrawCode()
{
    if (!m_pCodeLayout)
        return;

    DrawTextDecoder(D2D1::ColorF(0.7059f, 0.7059f, 0.7059f, 1.0f), m_CodeAnimProgress);
}

void Renderer::DrawTextFromLayout(
    const Microsoft::WRL::ComPtr<IDWriteTextLayout>& layout,
    const D2D1::ColorF& color,
    const D2D1_POINT_2F& position)
{
    if (!layout || !m_pReusableBrush)
        return;

    m_pReusableBrush->SetColor(color);
    m_pD2DContext->DrawTextLayout(
        D2D1::Point2F(position.x, position.y),
        layout.Get(),
        m_pReusableBrush.Get()
    );
}

Microsoft::WRL::ComPtr<IDWriteTextLayout> Renderer::GetTextMetrics(
    DWRITE_TEXT_METRICS* metrics,
    const std::wstring& text,
    const std::wstring& fontFamily,
    const float& fontSize,
    const DWRITE_FONT_WEIGHT& weight)
{
    CreateTextFormat(fontFamily, fontSize, weight);
    if (!m_pCurrentTextFormat)
        return nullptr;

    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_pDWriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        m_pCurrentTextFormat.Get(),
        3840.0f - 100.0f,
        2160.0f - 250.0f,
        &layout
    );
    if (FAILED(hr) || !layout)
    {
        PrintHR("CreateTextLayout", hr);
        return nullptr;
    }

    DWRITE_TEXT_METRICS m{};
    hr = layout->GetMetrics(&m);
    if (FAILED(hr))
    {
        PrintHR("GetMetrics", hr);
        return nullptr;
    }

    *metrics = m;
    return layout;
}

bool Renderer::LoadBackgroundTexture()
{
    Microsoft::WRL::ComPtr<ID3D11Resource> texRes;
    HRESULT hr = DirectX::CreateWICTextureFromFile(
        m_pD3DDevice.Get(),
        m_pD3DContext.Get(),
        L"../in/bg.png",
        texRes.GetAddressOf(),
        m_pBackgroundSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        PrintHR("CreateWICTextureFromFile(bg.png)", hr);
        return false;
    }

    hr = texRes.As(&m_pBackgroundTex);
    if (FAILED(hr))
    {
        PrintHR("Query ID3D11Texture2D", hr);
        return false;
    }

    return true;
}

bool Renderer::LoadBlurredTexture()
{
    Microsoft::WRL::ComPtr<ID3D11Resource> texRes;
    HRESULT hr = DirectX::CreateWICTextureFromFile(
        m_pD3DDevice.Get(),
        m_pD3DContext.Get(),
        L"../in/bg_blurred.png",
        texRes.GetAddressOf(),
        m_pBlurredSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        PrintHR("CreateWICTextureFromFile(bg_blurred.png)", hr);
        return false;
    }

    hr = texRes.As(&m_pBlurredTex);
    if (FAILED(hr))
    {
        PrintHR("Query ID3D11Texture2D", hr);
        return false;
    }

    return true;
}

void Renderer::InitDecoderStates()
{
    m_CharStates.clear();
    m_CharStates.reserve(m_Code.size());

    const uint32_t n = static_cast<uint32_t>(m_Code.size());
    if (n == 0)
        return;

    const float step = 1.0f / std::max(1u, n);

    for (uint32_t i = 0; i < n; ++i)
    {
        CharState s{};
        s.c = m_Code[i];
        s.bIsNewline = (s.c == L'\n');
        s.bIsWhitespace = (s.c == L' ' || s.c == L'\t');
        s.start = i * step;
        m_CharStates.push_back(s);
    }
}

void Renderer::DrawTextDecoder(const D2D1::ColorF& /*color*/, float animProgress)
{
    if (!m_pCodeLayout)
        return;

    CreateTextFormat(L"Consolas ligaturized v3", 72.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (!m_pCurrentTextFormat)
        return;

    const uint32_t n = static_cast<uint32_t>(m_CharStates.size());
    if (n == 0)
        return;

    const float fadeDur = 0.01f;

    auto CharProgress01 = [&](uint32_t i) -> float
        {
            const float s = m_CharStates[i].start;

            if (m_CharStates[i].bIsWhitespace || m_CharStates[i].bIsNewline)
                return (animProgress >= s) ? 1.0f : 0.0f;

            if (animProgress <= s)
                return 0.0f;
            if (animProgress >= s + fadeDur)
                return 1.0f;

            const float p = (animProgress - s) / fadeDur;
            return std::clamp(p, 0.0f, 1.0f);
        };

    uint32_t prefixLen = 0;
    while (prefixLen < n && CharProgress01(prefixLen) >= 1.0f)
        ++prefixLen;

    if (prefixLen > 0)
    {
        std::wstring prefix = m_Code.substr(0, prefixLen);

        Microsoft::WRL::ComPtr<IDWriteTextLayout> prefixLayout;
        HRESULT hr = m_pDWriteFactory->CreateTextLayout(
            prefix.c_str(),
            prefixLen,
            m_pCurrentTextFormat.Get(),
            3840.0f - 100.0f,
            2160.0f - 250.0f,
            &prefixLayout
        );
        if (SUCCEEDED(hr) && prefixLayout)
        {
            for (const Token& token : m_Tokens)
            {
                const uint32_t ts = token.start;
                const uint32_t te = token.start + token.length;
                if (token.length == 0)
                    continue;
                if (ts >= prefixLen)
                    continue;

                const uint32_t is = ts;
                const uint32_t ie = std::min(te, prefixLen);
                if (ie > is)
                {
                    DWRITE_TEXT_RANGE r{};
                    r.startPosition = is;
                    r.length = ie - is;
                    prefixLayout->SetDrawingEffect(SyntaxHighlighter::GetBrush(token).Get(), r);
                }
            }

            if (m_pReusableBrush)
            {
                m_pReusableBrush->SetColor(Colors::Other);
                m_pD2DContext->DrawTextLayout(
                    D2D1::Point2F(m_CodePosition.x, m_CodePosition.y),
                    prefixLayout.Get(),
                    m_pReusableBrush.Get()
                );
            }
        }
    }

    if (prefixLen >= n)
        return;

    const float p01 = CharProgress01(prefixLen);
    if (p01 <= 0.0f)
        return;

    const float alpha = EaseOutCubic(p01);

    float hitX = 0.0f, hitY = 0.0f;
    DWRITE_HIT_TEST_METRICS hit{};
    HRESULT hr = m_pCodeLayout->HitTestTextPosition(prefixLen, FALSE, &hitX, &hitY, &hit);
    if (FAILED(hr))
        return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> baseBrush;

    Microsoft::WRL::ComPtr<IUnknown> unk = Brushes::Other;
    for (const Token& token : m_Tokens)
    {
        const uint32_t ts = token.start;
        const uint32_t te = token.start + token.length;
        if (token.length == 0)
            continue;
        if (prefixLen >= ts && prefixLen < te)
        {
            unk = SyntaxHighlighter::GetBrush(token);
            break;
        }
    }

    unk.As(&baseBrush);

    if (!baseBrush)
        baseBrush = Brushes::Other;
    if (!baseBrush || !m_pReusableBrush)
        return;

    D2D1_COLOR_F c = baseBrush->GetColor();
    c.a *= alpha;
    m_pReusableBrush->SetColor(c);

    const wchar_t ch = m_Code[prefixLen];
    if (ch == L'\n' || ch == L' ' || ch == L'\t')
        return;

    const std::wstring one(1, ch);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> oneLayout;
    hr = m_pDWriteFactory->CreateTextLayout(
        one.c_str(),
        1,
        m_pCurrentTextFormat.Get(),
        std::max(1.0f, hit.width),
        std::max(1.0f, hit.height),
        &oneLayout
    );
    if (FAILED(hr) || !oneLayout)
        return;

    DWRITE_TEXT_RANGE r{ 0, 1 };
    oneLayout->SetDrawingEffect(m_pReusableBrush.Get(), r);

    m_pD2DContext->DrawTextLayout(
        D2D1::Point2F(m_CodePosition.x + hitX, m_CodePosition.y + hitY),
        oneLayout.Get(),
        m_pReusableBrush.Get()
    );
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
    D3D_FEATURE_LEVEL created{};

    HRESULT hr = D3D11CreateDevice(
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

    hr = D2D1CreateFactory(
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

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_pDWriteFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        PrintHR("DWriteCreateFactory", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateRenderTargets()
{
    D3D11_TEXTURE2D_DESC tex{};
    tex.Width = m_Width;
    tex.Height = m_Height;
    tex.MipLevels = 1;
    tex.ArraySize = 1;
    tex.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex.SampleDesc.Count = 1;
    tex.SampleDesc.Quality = 0;
    tex.Usage = D3D11_USAGE_DEFAULT;
    tex.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex.CPUAccessFlags = 0;
    tex.MiscFlags = 0;

    HRESULT hr = m_pD3DDevice->CreateTexture2D(&tex, nullptr, &m_pRenderTex);
    if (FAILED(hr))
    {
        PrintHR("CreateTexture2D(renderTex)", hr);
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = tex.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

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

    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX = 96.0f;
    bp.dpiY = 96.0f;
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;

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
    if (FAILED(hr))
    {
        if (errBlob)
            std::cerr << "Compute shader compile error:\n"
            << (const char*)errBlob->GetBufferPointer() << "\n";
        PrintHR("D3DCompileFromFile(ShapeCS.hlsl)", hr);
        return false;
    }

    hr = m_pD3DDevice->CreateComputeShader(
        csBlob->GetBufferPointer(),
        csBlob->GetBufferSize(),
        nullptr,
        &m_pCS
    );
    if (FAILED(hr))
    {
        PrintHR("CreateComputeShader", hr);
        return false;
    }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(CSConstants);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_pD3DDevice->CreateBuffer(&bd, nullptr, &m_pCSConstants);
    if (FAILED(hr))
    {
        PrintHR("CreateBuffer(CSConstants)", hr);
        return false;
    }

    return true;
}
