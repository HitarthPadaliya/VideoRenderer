#include "VideoEncoder.h"

#include <d3dcompiler.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")


VideoEncoder::VideoEncoder
(
    const std::string& outputPath,
    const uint16_t& width,
    const uint16_t& height,
    const uint8_t& fps,
    const uint32_t& bitrate
)
    : m_OutputPath(outputPath)
    , m_Width(width)
    , m_Height(height)
    , m_FPS(fps)
    , m_Bitrate(bitrate)
{}

VideoEncoder::~VideoEncoder()
{
    Finalize();
}


bool VideoEncoder::Initialize(ID3D11Device* pD3D11Device)
{
    if (m_Initialized)
        return true;

    m_pD3D11Device = pD3D11Device;
    pD3D11Device->GetImmediateContext(&m_pD3D11Context);

    HRESULT hr = pD3D11Device->QueryInterface(__uuidof(ID3D11Device3),
        (void**)m_pD3D11Device3.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !m_pD3D11Device3)
    {
        std::cerr << "ID3D11Device3 not available. Use Windows 10 SDK + D3D11.3 runtime.\n";
        return false;
    }

    if (!InitializeHardwareContext(pD3D11Device))
    {
        std::cerr << "Failed to initialize hardware context\n";
        return false;
    }

    if (!InitializeConverter())
    {
        std::cerr << "Failed to initialize converter\n";
        return false;
    }

    if (!InitializeEncoder())
    {
        std::cerr << "Failed to initialize encoder\n";
        return false;
    }

    if (!InitializeMuxer())
    {
        std::cerr << "Failed to initialize muxer\n";
        return false;
    }

    m_Initialized = true;
    std::cout << "VideoEncoder initialized\n";
    return true;
}

bool VideoEncoder::InitializeHardwareContext(ID3D11Device* pD3D11Device)
{
    m_pHWDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!m_pHWDeviceCtx)
    {
        std::cerr << "Failed to allocate D3D11VA device context\n";
        return false;
    }

    AVHWDeviceContext* deviceCtx = (AVHWDeviceContext*)m_pHWDeviceCtx->data;
    AVD3D11VADeviceContext* d3d11Ctx = (AVD3D11VADeviceContext*)deviceCtx->hwctx;

    d3d11Ctx->device = pD3D11Device;
    pD3D11Device->AddRef();

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pD3D11Context;
    pD3D11Device->GetImmediateContext(&pD3D11Context);
    d3d11Ctx->device_context = pD3D11Context.Get();
    pD3D11Context->AddRef();

    d3d11Ctx->lock_ctx = (void*)1;
    d3d11Ctx->lock = [](void*) {};
    d3d11Ctx->unlock = [](void*) {};

    int ret = av_hwdevice_ctx_init(m_pHWDeviceCtx);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to initialize hardware device context: " << errbuf << "\n";
        av_buffer_unref(&m_pHWDeviceCtx);
        return false;
    }

    m_pHWFramesCtx = av_hwframe_ctx_alloc(m_pHWDeviceCtx);
    if (!m_pHWFramesCtx)
    {
        std::cerr << "Failed to allocate hardware frames context\n";
        return false;
    }

    AVHWFramesContext* framesCtx    = (AVHWFramesContext*)m_pHWFramesCtx->data;
    framesCtx->format               = AV_PIX_FMT_D3D11;
    framesCtx->sw_format            = AV_PIX_FMT_P010LE;
    framesCtx->width                = m_Width;
    framesCtx->height               = m_Height;
    framesCtx->initial_pool_size    = 0;

    ret = av_hwframe_ctx_init(m_pHWFramesCtx);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to initialize hardware frames context: " << errbuf << "\n";
        av_buffer_unref(&m_pHWFramesCtx);
        return false;
    }

    return true;
}

bool VideoEncoder::InitializeConverter()
{
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width               = (UINT)m_Width;
    texDesc.Height              = (UINT)m_Height;
    texDesc.MipLevels           = 1;
    texDesc.ArraySize           = 1;
    texDesc.Format              = DXGI_FORMAT_P010;
    texDesc.SampleDesc.Count    = 1;
    texDesc.Usage               = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags      = 0;
    texDesc.MiscFlags           = 0;

    HRESULT hr = m_pD3D11Device->CreateTexture2D(&texDesc, nullptr, &m_pP010Texture);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create P010 texture: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 uavY {};
    uavY.Format                 = DXGI_FORMAT_R16_UINT;
    uavY.ViewDimension          = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavY.Texture2D.MipSlice     = 0;
    uavY.Texture2D.PlaneSlice   = 0;

    hr = m_pD3D11Device3->CreateUnorderedAccessView1(m_pP010Texture.Get(), &uavY,
        m_pOutputUAV_Y.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "Failed to create UAV (Y plane): 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 uavUV {};
    uavUV.Format                = DXGI_FORMAT_R16G16_UINT;
    uavUV.ViewDimension         = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavUV.Texture2D.MipSlice    = 0;
    uavUV.Texture2D.PlaneSlice  = 1;

    hr = m_pD3D11Device3->CreateUnorderedAccessView1(m_pP010Texture.Get(), &uavUV,
        m_pOutputUAV_UV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "Failed to create UAV (UV plane): 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    const char* csCode = R"(
Texture2D<float4> InputTexture : register(t0);

RWTexture2D<uint>  OutY  : register(u0);
RWTexture2D<uint2> OutUV : register(u1);

cbuffer Constants : register(b0)
{
    uint2 Resolution;
    uint2 Padding;
};

#define INPUT_IS_LINEAR 0

float3 LinearToRec709(float3 x)
{
    x = max(x, 0.0.xxx);
    float3 a = 4.5 * x;
    float3 b = 1.099 * pow(x, 0.45) - 0.099;
    return (x < 0.018) ? a : b;
}

float3 ToRec709Prime(float3 rgb)
{
#if INPUT_IS_LINEAR
    return LinearToRec709(rgb);
#else
    return rgb; // already R'G'B'
#endif
}

uint QuantizeY10_Limited(precise float y01)
{
    precise float v = saturate(y01) * 876.0 + 64.0;
    uint q = (uint)floor(v + 0.5);
    return clamp(q, 64u, 940u);
}

uint QuantizeC10_Limited(precise float cMinusHalfToHalf)
{
    precise float c = clamp(cMinusHalfToHalf, -0.5, 0.5);
    precise float v = c * 896.0 + 512.0;
    uint q = (uint)floor(v + 0.5);
    return clamp(q, 64u, 960u);
}

uint PackP010_10bitCode(uint code10)
{
    return (code10 & 1023u) << 6;
}

void RGBToYCbCr709_Prime(float3 rgbIn, out precise float Y, out precise float Cb, out precise float Cr)
{
    precise float3 rgbp = saturate(ToRec709Prime(rgbIn));

    const precise float Kr = 0.2126;
    const precise float Kb = 0.0722;
    const precise float Kg = 1.0 - Kr - Kb;

    Y = Kr * rgbp.r + Kg * rgbp.g + Kb * rgbp.b;

    Cb = (rgbp.b - Y) / (2.0 * (1.0 - Kb)); // == / 1.8556
    Cr = (rgbp.r - Y) / (2.0 * (1.0 - Kr)); // == / 1.5748
}

[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint x = tid.x;
    uint y = tid.y;
    if (x >= Resolution.x || y >= Resolution.y)
        return;

    float3 rgb = InputTexture[uint2(x, y)].rgb;

    precise float Yp, Cb, Cr;
    RGBToYCbCr709_Prime(rgb, Yp, Cb, Cr);

    OutY[uint2(x, y)] = PackP010_10bitCode(QuantizeY10_Limited(Yp));

    if (((x & 1) == 0) && ((y & 1) == 0))
    {
        uint x1 = min(x + 1, Resolution.x - 1);
        uint y1 = min(y + 1, Resolution.y - 1);

        precise float Y00, U00, V00;
        precise float Y10, U10, V10;
        precise float Y01, U01, V01;
        precise float Y11, U11, V11;

        RGBToYCbCr709_Prime(InputTexture[uint2(x,  y )].rgb, Y00, U00, V00);
        RGBToYCbCr709_Prime(InputTexture[uint2(x1, y )].rgb, Y10, U10, V10);
        RGBToYCbCr709_Prime(InputTexture[uint2(x,  y1)].rgb, Y01, U01, V01);
        RGBToYCbCr709_Prime(InputTexture[uint2(x1, y1)].rgb, Y11, U11, V11);

        precise float uAvg = (U00 + U10 + U01 + U11) * 0.25;
        precise float vAvg = (V00 + V10 + V01 + V11) * 0.25;

        OutUV[uint2(x >> 1, y >> 1)] =
            uint2(PackP010_10bitCode(QuantizeC10_Limited(uAvg)),
                  PackP010_10bitCode(QuantizeC10_Limited(vAvg)));
    }
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    hr = D3DCompile(csCode, std::strlen(csCode), nullptr, nullptr, nullptr,
        "CSMain", "cs_5_0", 0, 0, &csBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
            std::cerr << "CS compile error: " << (const char*)errBlob->GetBufferPointer() << "\n";
        std::cerr << "Failed to compile compute shader: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    hr = m_pD3D11Device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(),
        nullptr, &m_pConvertCS);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create compute shader: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    ConvertConstants c {};
    c.m_Resolution[0]   = m_Width;
    c.m_Resolution[1]   = m_Height;
    c.m_Padding[0]      = 0;
    c.m_Padding[1]      = 0;

    D3D11_BUFFER_DESC cbDesc {};
    cbDesc.ByteWidth        = sizeof(ConvertConstants);
    cbDesc.Usage            = D3D11_USAGE_IMMUTABLE;
    cbDesc.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags   = 0;

    D3D11_SUBRESOURCE_DATA initData {};
    initData.pSysMem = &c;

    hr = m_pD3D11Device->CreateBuffer(&cbDesc, &initData, &m_pConvertConstants);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create immutable constant buffer: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    std::cout << "Color converter initialized (RGBA16F -> P010, plane UAVs, cached SRV, immutable constants)\n";
    return true;
}

bool VideoEncoder::EnsureInputSRV(ID3D11Texture2D* pRGBATexture)
{
    if (!pRGBATexture)
        return false;

    if (m_pCachedInputTex.Get() == pRGBATexture && m_pInputSRV)
        return true;

    m_pCachedInputTex.Reset();
    m_pInputSRV.Reset();
    m_pCachedInputTex = pRGBATexture;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format                      = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels         = 1;
    srvDesc.Texture2D.MostDetailedMip   = 0;

    HRESULT hr = m_pD3D11Device->CreateShaderResourceView(pRGBATexture, &srvDesc, &m_pInputSRV);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create SRV: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    return true;
}

ID3D11Texture2D* VideoEncoder::ConvertToP010(ID3D11Texture2D* pRGBATexture)
{
    if (!EnsureInputSRV(pRGBATexture))
        return nullptr;

    m_pD3D11Context->CSSetShader(m_pConvertCS.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { m_pInputSRV.Get() };
    m_pD3D11Context->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = { m_pOutputUAV_Y.Get(), m_pOutputUAV_UV.Get() };
    UINT initialCounts[] = { 0, 0 };
    m_pD3D11Context->CSSetUnorderedAccessViews(0, 2, uavs, initialCounts);

    ID3D11Buffer* cbs[] = { m_pConvertConstants.Get() };
    m_pD3D11Context->CSSetConstantBuffers(0, 1, cbs);

    m_pD3D11Context->Dispatch(((m_Width + 31) / 32), ((m_Height + 31) / 32), 1);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    m_pD3D11Context->CSSetShaderResources(0, 1, nullSRV);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    m_pD3D11Context->CSSetUnorderedAccessViews(0, 2, nullUAVs, initialCounts);

    ID3D11Buffer* nullCB[] = { nullptr };
    m_pD3D11Context->CSSetConstantBuffers(0, 1, nullCB);

    m_pD3D11Context->CSSetShader(nullptr, nullptr, 0);

    return m_pP010Texture.Get();
}

bool VideoEncoder::InitializeEncoder()
{
    m_pCodec = avcodec_find_encoder_by_name("hevc_nvenc");
    if (!m_pCodec)
    {
        std::cerr << "hevc_nvenc encoder not found. Ensure FFmpeg was built with NVENC support.\n";
        return false;
    }

    m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
    if (!m_pCodecCtx)
    {
        std::cerr << "Failed to allocate codec context\n";
        return false;
    }

    m_pCodecCtx->width              = m_Width;
    m_pCodecCtx->height             = m_Height;
    m_pCodecCtx->time_base          = AVRational { 1, m_FPS };
    m_pCodecCtx->framerate          = AVRational { m_FPS, 1 };
    m_pCodecCtx->pix_fmt            = AV_PIX_FMT_D3D11;
    m_pCodecCtx->bit_rate           = m_Bitrate;
    m_pCodecCtx->hw_frames_ctx      = av_buffer_ref(m_pHWFramesCtx);
    m_pCodecCtx->color_range        = AVCOL_RANGE_MPEG;
    m_pCodecCtx->colorspace         = AVCOL_SPC_BT709;
    m_pCodecCtx->color_primaries    = AVCOL_PRI_BT709;
    m_pCodecCtx->color_trc          = AVCOL_TRC_BT709;

    av_opt_set(m_pCodecCtx->priv_data, "preset", "p7", 0);
    av_opt_set(m_pCodecCtx->priv_data, "tune", "hq", 0);
    av_opt_set(m_pCodecCtx->priv_data, "profile", "main10", 0);
    av_opt_set(m_pCodecCtx->priv_data, "rc", "vbr", 0);
    av_opt_set_int(m_pCodecCtx->priv_data, "cq", m_CQ, 0);
    av_opt_set_int(m_pCodecCtx->priv_data, "rc-lookahead", m_Lookahead, 0);

    int ret = avcodec_open2(m_pCodecCtx, m_pCodec, nullptr);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to open encoder: " << errbuf << "\n";
        return false;
    }

    return true;
}

bool VideoEncoder::InitializeMuxer()
{
    int ret = avformat_alloc_output_context2(&m_pFormatCtx, nullptr, nullptr, m_OutputPath.c_str());
    if (!m_pFormatCtx)
    {
        std::cerr << "Failed to allocate output format context\n";
        return false;
    }

    m_pStream = avformat_new_stream(m_pFormatCtx, nullptr);
    if (!m_pStream)
    {
        std::cerr << "Failed to create video stream\n";
        return false;
    }

    m_pStream->time_base = m_pCodecCtx->time_base;

    ret = avcodec_parameters_from_context(m_pStream->codecpar, m_pCodecCtx);
    if (ret < 0)
    {
        std::cerr << "Failed to copy codec parameters\n";
        return false;
    }

    if (!(m_pFormatCtx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&m_pFormatCtx->pb, m_OutputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to open output file: " << errbuf << "\n";
            return false;
        }
    }

    ret = avformat_write_header(m_pFormatCtx, nullptr);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to write header: " << errbuf << "\n";
        return false;
    }

    return true;
}

AVFrame* VideoEncoder::WrapD3D11Texture(ID3D11Texture2D* pTexture)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return nullptr;

    frame->format           = AV_PIX_FMT_D3D11;
    frame->width            = m_Width;
    frame->height           = m_Height;
    frame->hw_frames_ctx    = av_buffer_ref(m_pHWFramesCtx);
    frame->data[0]          = (uint8_t*)pTexture;
    frame->data[1]          = 0;

    pTexture->AddRef();
    frame->buf[0] = av_buffer_create
    (
        (uint8_t*)pTexture, 0,
        [](void*, uint8_t* data)
        {
            ID3D11Texture2D* tex = (ID3D11Texture2D*)data;
            tex->Release();
        },
        nullptr, 0
    );

    frame->pts = m_FrameCount++;
    return frame;
}

bool VideoEncoder::WritePacket(AVPacket* pkt)
{
    av_packet_rescale_ts(pkt, m_pCodecCtx->time_base, m_pStream->time_base);
    pkt->stream_index = m_pStream->index;

    int ret = av_interleaved_write_frame(m_pFormatCtx, pkt);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Error writing packet: " << errbuf << "\n";
        return false;
    }

    return true;
}

bool VideoEncoder::EncodeFrame(ID3D11Texture2D* pRGBATexture)
{
    if (!m_Initialized)
        return false;

    ID3D11Texture2D* pP010Texture = ConvertToP010(pRGBATexture);
    if (!pP010Texture)
    {
        std::cerr << "Failed to convert RGBA16F to P010\n";
        return false;
    }

    AVFrame* frame = WrapD3D11Texture(pP010Texture);
    if (!frame) {
        std::cerr << "Failed to wrap D3D11 texture\n";
        return false;
    }

    int ret = avcodec_send_frame(m_pCodecCtx, frame);
    av_frame_free(&frame);

    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Error sending frame: " << errbuf << "\n";
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        std::cerr << "Failed to allocate packet\n";
        return false;
    }

    while (true)
    {
        ret = avcodec_receive_packet(m_pCodecCtx, pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Error receiving packet: " << errbuf << "\n";
            av_packet_free(&pkt);
            return false;
        }

        if (!WritePacket(pkt))
        {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            return false;
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    return true;
}

bool VideoEncoder::Finalize()
{
    if (!m_Initialized)
        return true;

    avcodec_send_frame(m_pCodecCtx, nullptr);

    AVPacket* pkt = av_packet_alloc();
    if (pkt)
    {
        int ret = 0;
        while ((ret = avcodec_receive_packet(m_pCodecCtx, pkt)) >= 0)
        {
            WritePacket(pkt);
            av_packet_unref(pkt);
        }

        av_packet_free(&pkt);
    }

    if (m_pFormatCtx)
    {
        av_write_trailer(m_pFormatCtx);
        if (!(m_pFormatCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_pFormatCtx->pb);
    }

    if (m_pCodecCtx)        avcodec_free_context(&m_pCodecCtx);
    if (m_pFormatCtx)       avformat_free_context(m_pFormatCtx);
    if (m_pHWFramesCtx)     av_buffer_unref(&m_pHWFramesCtx);
    if (m_pHWDeviceCtx)     av_buffer_unref(&m_pHWDeviceCtx);

    m_pCachedInputTex.Reset();
    m_pInputSRV.Reset();

    m_pOutputUAV_Y.Reset();
    m_pOutputUAV_UV.Reset();
    m_pConvertCS.Reset();
    m_pConvertConstants.Reset();
    m_pP010Texture.Reset();
    m_pD3D11Device3.Reset();

    m_pD3D11Context.Reset();
    m_pD3D11Device.Reset();

    m_Initialized = false;

    std::cout << "Encoder finalized\n";
    return true;
}
