#include "VideoEncoder.h"

#include <d3dcompiler.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

VideoEncoder::VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate)
    : m_outputPath(outputPath),
    m_width(width),
    m_height(height),
    m_fps(fps),
    m_bitrate(bitrate) {
}

VideoEncoder::~VideoEncoder() {
    Finalize();
}

bool VideoEncoder::Initialize(ID3D11Device* pD3D11Device) {
    if (m_initialized) return true;

    m_d3d11Device = pD3D11Device;
    pD3D11Device->GetImmediateContext(&m_d3d11Context);

    // Needed for CreateUnorderedAccessView1 + PlaneSlice UAVs (P010 planes).
    HRESULT hr = pD3D11Device->QueryInterface(__uuidof(ID3D11Device3),
        (void**)m_d3d11Device3.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !m_d3d11Device3) {
        std::cerr << "ID3D11Device3 not available. Use Windows 10 SDK + D3D11.3 runtime.\n";
        return false;
    }

    if (!InitializeHardwareContext(pD3D11Device)) {
        std::cerr << "Failed to initialize hardware context\n";
        return false;
    }

    if (!InitializeConverter(pD3D11Device)) {
        std::cerr << "Failed to initialize converter\n";
        return false;
    }

    if (!InitializeEncoder()) {
        std::cerr << "Failed to initialize encoder\n";
        return false;
    }

    if (!InitializeMuxer()) {
        std::cerr << "Failed to initialize muxer\n";
        return false;
    }

    m_initialized = true;

    std::cout << "VideoEncoder initialized (D3D11 RGBA16F->P010 -> NVENC via libavcodec)\n"
        << " Output: " << m_outputPath << "\n"
        << " Resolution: " << m_width << "x" << m_height << " @" << m_fps << "fps\n"
        << " Codec: HEVC Main10 (CQ=" << m_cq << ")\n";

    return true;
}

bool VideoEncoder::InitializeHardwareContext(ID3D11Device* pD3D11Device) {
    // Manually create D3D11VA hardware device context
    m_hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!m_hwDeviceCtx) {
        std::cerr << "Failed to allocate D3D11VA device context\n";
        return false;
    }

    AVHWDeviceContext* deviceCtx = (AVHWDeviceContext*)m_hwDeviceCtx->data;
    AVD3D11VADeviceContext* d3d11Ctx = (AVD3D11VADeviceContext*)deviceCtx->hwctx;

    // Set our existing D3D11 device
    d3d11Ctx->device = pD3D11Device;
    pD3D11Device->AddRef();

    // Get device context
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pD3D11Context;
    pD3D11Device->GetImmediateContext(&pD3D11Context);
    d3d11Ctx->device_context = pD3D11Context.Get();
    pD3D11Context->AddRef();

    // Set lock/unlock functions
    d3d11Ctx->lock_ctx = (void*)1;
    d3d11Ctx->lock = [](void*) {};
    d3d11Ctx->unlock = [](void*) {};

    int ret = av_hwdevice_ctx_init(m_hwDeviceCtx);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to initialize hardware device context: " << errbuf << "\n";
        av_buffer_unref(&m_hwDeviceCtx);
        return false;
    }

    // Create hardware frames context for P010
    m_hwFramesCtx = av_hwframe_ctx_alloc(m_hwDeviceCtx);
    if (!m_hwFramesCtx) {
        std::cerr << "Failed to allocate hardware frames context\n";
        return false;
    }

    AVHWFramesContext* framesCtx = (AVHWFramesContext*)m_hwFramesCtx->data;
    framesCtx->format = AV_PIX_FMT_D3D11;
    framesCtx->sw_format = AV_PIX_FMT_P010LE;
    framesCtx->width = m_width;
    framesCtx->height = m_height;
    framesCtx->initial_pool_size = 0;

    ret = av_hwframe_ctx_init(m_hwFramesCtx);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to initialize hardware frames context: " << errbuf << "\n";
        av_buffer_unref(&m_hwFramesCtx);
        return false;
    }

    return true;
}

bool VideoEncoder::InitializeConverter(ID3D11Device* /*pD3D11Device*/) {
    // Correct P010 texture dims: width x height (planes accessed via PlaneSlice UAVs).
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = (UINT)m_width;
    texDesc.Height = (UINT)m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_P010;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    HRESULT hr = m_d3d11Device->CreateTexture2D(&texDesc, nullptr, &m_p010Texture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create P010 texture: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    // UAV for plane 0 (Y): R16_UINT
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 uavY{};
    uavY.Format = DXGI_FORMAT_R16_UINT;
    uavY.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavY.Texture2D.MipSlice = 0;
    uavY.Texture2D.PlaneSlice = 0;

    hr = m_d3d11Device3->CreateUnorderedAccessView1(m_p010Texture.Get(), &uavY,
        m_outputUAV_Y.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "Failed to create UAV (Y plane): 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    // UAV for plane 1 (UV): R16G16_UINT
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 uavUV{};
    uavUV.Format = DXGI_FORMAT_R16G16_UINT;
    uavUV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavUV.Texture2D.MipSlice = 0;
    uavUV.Texture2D.PlaneSlice = 1;

    hr = m_d3d11Device3->CreateUnorderedAccessView1(m_p010Texture.Get(), &uavUV,
        m_outputUAV_UV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "Failed to create UAV (UV plane): 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    // Compute shader for RGBA16F -> P010 (BT.709 limited)
    const char* csCode = R"(
Texture2D<float4> InputTexture : register(t0);

RWTexture2D<uint>  OutY  : register(u0);   // R16_UINT UAV (PlaneSlice=0)
RWTexture2D<uint2> OutUV : register(u1);   // R16G16_UINT UAV (PlaneSlice=1)

cbuffer Constants : register(b0) {
    uint2 Resolution;
    uint2 Padding;
};

float3 LinearToRec709(float3 x)
{
    x = max(x, 0.0.xxx);
    float3 a = 4.5 * x;
    float3 b = 1.099 * pow(x, 0.45) - 0.099;
    return (x < 0.018) ? a : b;
}

float3 RGBToYCbCr709_Limited(float3 rgbLinear)
{
    float3 rgb = saturate(rgbLinear);
    float3 rgbp = LinearToRec709(rgb);

    float Y  = 0.2126 * rgbp.r + 0.7152 * rgbp.g + 0.0722 * rgbp.b;
    float Cb = (rgbp.b - Y) / 1.8556;
    float Cr = (rgbp.r - Y) / 1.5748;

    float Yl  = (16.0/255.0)  + (219.0/255.0) * Y;
    float Cbl = (128.0/255.0) + (224.0/255.0) * Cb;
    float Crl = (128.0/255.0) + (224.0/255.0) * Cr;

    return float3(saturate(Yl), saturate(Cbl), saturate(Crl));
}

uint PackP010_10bit(float v01)
{
    uint v10 = (uint)round(saturate(v01) * 1023.0);
    return (v10 << 6); // P010 stores sample in bits 15:6
}

[numthreads(16,16,1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint x = tid.x;
    uint y = tid.y;
    if (x >= Resolution.x || y >= Resolution.y)
        return;

    float3 ycc = RGBToYCbCr709_Limited(InputTexture[uint2(x,y)].rgb);
    OutY[uint2(x,y)] = PackP010_10bit(ycc.x);

    // 4:2:0 UV on even pixels only
    if (((x & 1) == 0) && ((y & 1) == 0))
    {
        uint x1 = min(x + 1, Resolution.x - 1);
        uint y1 = min(y + 1, Resolution.y - 1);

        float3 ycc00 = RGBToYCbCr709_Limited(InputTexture[uint2(x,  y )].rgb);
        float3 ycc10 = RGBToYCbCr709_Limited(InputTexture[uint2(x1, y )].rgb);
        float3 ycc01 = RGBToYCbCr709_Limited(InputTexture[uint2(x,  y1)].rgb);
        float3 ycc11 = RGBToYCbCr709_Limited(InputTexture[uint2(x1, y1)].rgb);

        float u = (ycc00.y + ycc10.y + ycc01.y + ycc11.y) * 0.25;
        float v = (ycc00.z + ycc10.z + ycc01.z + ycc11.z) * 0.25;

        OutUV[uint2(x >> 1, y >> 1)] = uint2(PackP010_10bit(u), PackP010_10bit(v));
    }
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    hr = D3DCompile(csCode, std::strlen(csCode), nullptr, nullptr, nullptr,
        "CSMain", "cs_5_0", 0, 0, &csBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            std::cerr << "CS compile error: " << (const char*)errBlob->GetBufferPointer() << "\n";
        }
        std::cerr << "Failed to compile compute shader: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    hr = m_d3d11Device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(),
        nullptr, &m_convertCS);
    if (FAILED(hr)) {
        std::cerr << "Failed to create compute shader: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    // ---- CHURN REMOVAL: immutable constant buffer (no per-frame Map/Unmap) ----
    ConvertConstants c{};
    c.Resolution[0] = (UINT)m_width;
    c.Resolution[1] = (UINT)m_height;
    c.Padding[0] = 0;
    c.Padding[1] = 0;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(ConvertConstants);
    cbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = &c;

    hr = m_d3d11Device->CreateBuffer(&cbDesc, &initData, &m_convertConstants);
    if (FAILED(hr)) {
        std::cerr << "Failed to create immutable constant buffer: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    std::cout << "Color converter initialized (RGBA16F -> P010, plane UAVs, cached SRV, immutable constants)\n";
    return true;
}

bool VideoEncoder::EnsureInputSRV(ID3D11Texture2D* pRGBATexture) {
    if (!pRGBATexture) return false;

    // If same texture pointer as last time, reuse SRV (no per-frame churn).
    if (m_cachedInputTex.Get() == pRGBATexture && m_inputSRV) {
        return true;
    }

    // Update cached texture ref + recreate SRV once.
    m_cachedInputTex.Reset();
    m_inputSRV.Reset();
    m_cachedInputTex = pRGBATexture;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    HRESULT hr = m_d3d11Device->CreateShaderResourceView(pRGBATexture, &srvDesc, &m_inputSRV);
    if (FAILED(hr)) {
        std::cerr << "Failed to create SRV: 0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    return true;
}

ID3D11Texture2D* VideoEncoder::ConvertToP010(ID3D11Texture2D* pRGBATexture) {
    if (!EnsureInputSRV(pRGBATexture)) {
        return nullptr;
    }

    // Bind compute shader + resources
    m_d3d11Context->CSSetShader(m_convertCS.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { m_inputSRV.Get() };
    m_d3d11Context->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = { m_outputUAV_Y.Get(), m_outputUAV_UV.Get() };
    UINT initialCounts[] = { 0, 0 };
    m_d3d11Context->CSSetUnorderedAccessViews(0, 2, uavs, initialCounts);

    ID3D11Buffer* cbs[] = { m_convertConstants.Get() };
    m_d3d11Context->CSSetConstantBuffers(0, 1, cbs);

    // Dispatch
    UINT groupsX = (UINT)((m_width + 15) / 16);
    UINT groupsY = (UINT)((m_height + 15) / 16);
    m_d3d11Context->Dispatch(groupsX, groupsY, 1);

    // Unbind to avoid hazards for subsequent passes (safe default for sample code)
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    m_d3d11Context->CSSetShaderResources(0, 1, nullSRV);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    m_d3d11Context->CSSetUnorderedAccessViews(0, 2, nullUAVs, initialCounts);

    ID3D11Buffer* nullCB[] = { nullptr };
    m_d3d11Context->CSSetConstantBuffers(0, 1, nullCB);

    m_d3d11Context->CSSetShader(nullptr, nullptr, 0);

    return m_p010Texture.Get();
}

bool VideoEncoder::InitializeEncoder() {
    m_codec = avcodec_find_encoder_by_name("hevc_nvenc");
    if (!m_codec) {
        std::cerr << "hevc_nvenc encoder not found. Ensure FFmpeg was built with NVENC support.\n";
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        std::cerr << "Failed to allocate codec context\n";
        return false;
    }

    m_codecCtx->width = m_width;
    m_codecCtx->height = m_height;
    m_codecCtx->time_base = AVRational{ 1, m_fps };
    m_codecCtx->framerate = AVRational{ m_fps, 1 };
    m_codecCtx->pix_fmt = AV_PIX_FMT_D3D11;
    m_codecCtx->bit_rate = m_bitrate;
    m_codecCtx->hw_frames_ctx = av_buffer_ref(m_hwFramesCtx);

    // Optional: signal color info
    m_codecCtx->color_range = AVCOL_RANGE_MPEG;
    m_codecCtx->colorspace = AVCOL_SPC_BT709;
    m_codecCtx->color_primaries = AVCOL_PRI_BT709;
    m_codecCtx->color_trc = AVCOL_TRC_BT709;

    // NVENC options
    av_opt_set(m_codecCtx->priv_data, "preset", "p7", 0);
    av_opt_set(m_codecCtx->priv_data, "tune", "hq", 0);
    av_opt_set(m_codecCtx->priv_data, "profile", "main10", 0);
    av_opt_set(m_codecCtx->priv_data, "rc", "vbr", 0);
    av_opt_set_int(m_codecCtx->priv_data, "cq", m_cq, 0);
    av_opt_set_int(m_codecCtx->priv_data, "rc-lookahead", m_lookahead, 0);

    int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to open encoder: " << errbuf << "\n";
        return false;
    }

    return true;
}

bool VideoEncoder::InitializeMuxer() {
    int ret = avformat_alloc_output_context2(&m_formatCtx, nullptr, nullptr, m_outputPath.c_str());
    if (!m_formatCtx) {
        std::cerr << "Failed to allocate output format context\n";
        return false;
    }

    m_stream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_stream) {
        std::cerr << "Failed to create video stream\n";
        return false;
    }

    m_stream->time_base = m_codecCtx->time_base;

    ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecCtx);
    if (ret < 0) {
        std::cerr << "Failed to copy codec parameters\n";
        return false;
    }

    if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_formatCtx->pb, m_outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to open output file: " << errbuf << "\n";
            return false;
        }
    }

    ret = avformat_write_header(m_formatCtx, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to write header: " << errbuf << "\n";
        return false;
    }

    return true;
}

AVFrame* VideoEncoder::WrapD3D11Texture(ID3D11Texture2D* pTexture) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) return nullptr;

    frame->format = AV_PIX_FMT_D3D11;
    frame->width = m_width;
    frame->height = m_height;
    frame->hw_frames_ctx = av_buffer_ref(m_hwFramesCtx);

    frame->data[0] = (uint8_t*)pTexture;
    frame->data[1] = 0;

    pTexture->AddRef();
    frame->buf[0] = av_buffer_create(
        (uint8_t*)pTexture,
        0,
        [](void*, uint8_t* data) {
            ID3D11Texture2D* tex = (ID3D11Texture2D*)data;
            tex->Release();
        },
        nullptr,
        0
    );

    frame->pts = m_frameCount++;
    return frame;
}

bool VideoEncoder::WritePacket(AVPacket* pkt) {
    av_packet_rescale_ts(pkt, m_codecCtx->time_base, m_stream->time_base);
    pkt->stream_index = m_stream->index;

    int ret = av_interleaved_write_frame(m_formatCtx, pkt);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Error writing packet: " << errbuf << "\n";
        return false;
    }

    return true;
}

bool VideoEncoder::EncodeFrame(ID3D11Texture2D* pRGBATexture) {
    if (!m_initialized) return false;

    // Convert RGBA16F to P010 on GPU using compute shader
    ID3D11Texture2D* pP010Texture = ConvertToP010(pRGBATexture);
    if (!pP010Texture) {
        std::cerr << "Failed to convert RGBA16F to P010\n";
        return false;
    }

    // Wrap P010 texture in AVFrame
    AVFrame* frame = WrapD3D11Texture(pP010Texture);
    if (!frame) {
        std::cerr << "Failed to wrap D3D11 texture\n";
        return false;
    }

    int ret = avcodec_send_frame(m_codecCtx, frame);
    av_frame_free(&frame);

    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Error sending frame: " << errbuf << "\n";
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "Failed to allocate packet\n";
        return false;
    }

    while (true) {
        ret = avcodec_receive_packet(m_codecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        else if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Error receiving packet: " << errbuf << "\n";
            av_packet_free(&pkt);
            return false;
        }

        if (!WritePacket(pkt)) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            return false;
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    return true;
}

bool VideoEncoder::Finalize() {
    if (!m_initialized) return true;

    // Flush encoder
    avcodec_send_frame(m_codecCtx, nullptr);

    AVPacket* pkt = av_packet_alloc();
    if (pkt) {
        int ret = 0;
        while ((ret = avcodec_receive_packet(m_codecCtx, pkt)) >= 0) {
            WritePacket(pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    // Write trailer
    if (m_formatCtx) {
        av_write_trailer(m_formatCtx);
        if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_formatCtx->pb);
        }
    }

    // Cleanup FFmpeg
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
    if (m_formatCtx) avformat_free_context(m_formatCtx);
    if (m_hwFramesCtx) av_buffer_unref(&m_hwFramesCtx);
    if (m_hwDeviceCtx) av_buffer_unref(&m_hwDeviceCtx);

    // Cleanup D3D11
    m_cachedInputTex.Reset();
    m_inputSRV.Reset();

    m_outputUAV_Y.Reset();
    m_outputUAV_UV.Reset();
    m_convertCS.Reset();
    m_convertConstants.Reset();
    m_p010Texture.Reset();
    m_d3d11Device3.Reset();

    m_d3d11Context.Reset();
    m_d3d11Device.Reset();

    m_initialized = false;

    std::cout << "Encoder finalized. Total frames: " << m_frameCount << "\n";
    return true;
}
