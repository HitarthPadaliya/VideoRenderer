#pragma once

#include <d3d11.h>
#include <d3d11_3.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/hwcontext_d3d11va.h>
    #include <libavutil/opt.h>
}


struct ConvertConstants
{
    uint32_t m_Resolution[2];
    uint32_t m_Padding[2];
};


class VideoEncoder
{
    private:
        std::string m_OutputPath;
        uint16_t m_Width    = 0;
        uint16_t m_Height   = 0;
        uint8_t m_FPS       = 0;
        uint32_t m_Bitrate  = 0;

        int m_CQ = 18;
        int m_Lookahead = 32;

        AVBufferRef* m_pHWDeviceCtx      = nullptr;
        AVBufferRef* m_pHWFramesCtx      = nullptr;
        const AVCodec* m_pCodec          = nullptr;
        AVCodecContext* m_pCodecCtx      = nullptr;
        AVFormatContext* m_pFormatCtx    = nullptr;
        AVStream* m_pStream              = nullptr;

        Microsoft::WRL::ComPtr<ID3D11Device> m_pD3D11Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pD3D11Context;
        Microsoft::WRL::ComPtr<ID3D11Device3> m_pD3D11Device3;

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pConvertCS;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pInputSRV;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pCachedInputTex;

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView1> m_pOutputUAV_Y;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView1> m_pOutputUAV_UV;

        Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConvertConstants;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pP010Texture;

        int32_t m_FrameCount = 0;
        bool m_Initialized = false;


    public:
        VideoEncoder(const std::string& outputPath, const uint16_t& width, const uint16_t& height,
            const uint8_t& fps, const uint32_t& bitrate);
        ~VideoEncoder();
    
        bool Initialize(ID3D11Device* pD3D11Device);
        bool EncodeFrame(ID3D11Texture2D* pTexture);
        bool Finalize();
    

    private:
        bool InitializeHardwareContext(ID3D11Device* pD3D11Device);
        bool InitializeConverter();
        bool InitializeEncoder();
        bool InitializeMuxer();
    
        ID3D11Texture2D* ConvertToP010(ID3D11Texture2D* pRGBATexture);

        bool EnsureInputSRV(ID3D11Texture2D* pRGBATexture);
    
        AVFrame* WrapD3D11Texture(ID3D11Texture2D* pTexture);
        bool WritePacket(AVPacket* pkt);
};
