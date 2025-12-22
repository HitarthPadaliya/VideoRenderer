#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

class VideoEncoder {
public:
    VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate);
    ~VideoEncoder();

    bool Initialize();
    bool EncodeFrame(const uint8_t* bgraData, int stride);
    bool Finalize();

private:
    bool WritePacket(AVPacket* packet);

    std::string m_outputPath;
    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;
    int64_t m_frameCount;

    const AVCodec* m_codec;
    AVCodecContext* m_codecCtx;
    AVFormatContext* m_formatCtx;
    AVStream* m_stream;
    SwsContext* m_swsCtx;
    AVFrame* m_frame;
    AVPacket* m_packet;
};
