#pragma once

#include <d3d11.h>
#include <d3d11_3.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/opt.h>
}

class VideoEncoder {
public:
    VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate);
    ~VideoEncoder();

    bool Initialize(ID3D11Device* pD3D11Device);
    bool EncodeFrame(ID3D11Texture2D* pTexture);
    bool Finalize();

private:
    bool InitializeHardwareContext(ID3D11Device* pD3D11Device);
    bool InitializeConverter(ID3D11Device* pD3D11Device);
    bool InitializeEncoder();
    bool InitializeMuxer();

    ID3D11Texture2D* ConvertToP010(ID3D11Texture2D* pRGBATexture);

    // Creates/updates SRV only when input texture changes.
    bool EnsureInputSRV(ID3D11Texture2D* pRGBATexture);

    AVFrame* WrapD3D11Texture(ID3D11Texture2D* pTexture);
    bool WritePacket(AVPacket* pkt);

private:
    std::string m_outputPath;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_bitrate = 0;

    int m_cq = 18;
    int m_lookahead = 32;

    // FFmpeg objects
    AVBufferRef* m_hwDeviceCtx = nullptr;
    AVBufferRef* m_hwFramesCtx = nullptr;
    const AVCodec* m_codec = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    AVFormatContext* m_formatCtx = nullptr;
    AVStream* m_stream = nullptr;

    // D3D11
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;

    // D3D11.3 interface for CreateUnorderedAccessView1 + DESC1 (plane-slice UAVs)
    Microsoft::WRL::ComPtr<ID3D11Device3> m_d3d11Device3;

    // Converter resources
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_convertCS;

    // Cached input SRV (recreated only if input texture changes)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_inputSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cachedInputTex;

    // Output UAVs into P010 planes
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView1> m_outputUAV_Y;   // plane 0
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView1> m_outputUAV_UV;  // plane 1

    // Immutable constants buffer (no per-frame Map/Unmap)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_convertConstants;

    // Output P010 texture for encoder input
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_p010Texture;

    struct ConvertConstants {
        UINT Resolution[2];
        UINT Padding[2];
    };

    int64_t m_frameCount = 0;
    bool m_initialized = false;
};
