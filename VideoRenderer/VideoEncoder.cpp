#include "VideoEncoder.h"
#include <iostream>

VideoEncoder::VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate)
    : m_outputPath(outputPath)
    , m_width(width)
    , m_height(height)
    , m_fps(fps)
    , m_bitrate(bitrate)
    , m_frameCount(0)
    , m_codec(nullptr)
    , m_codecCtx(nullptr)
    , m_formatCtx(nullptr)
    , m_stream(nullptr)
    , m_swsCtx(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
{
}

VideoEncoder::~VideoEncoder() {
    if (m_packet) av_packet_free(&m_packet);
    if (m_frame) av_frame_free(&m_frame);
    if (m_swsCtx) sws_freeContext(m_swsCtx);
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
    if (m_formatCtx) {
        if (m_formatCtx->pb) avio_closep(&m_formatCtx->pb);
        avformat_free_context(m_formatCtx);
    }
}

bool VideoEncoder::Initialize() {
    // Find NVENC encoder
    m_codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!m_codec) {
        std::cerr << "h264_nvenc encoder not found" << std::endl;
        return false;
    }

    // Allocate codec context
    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }

    // Configure codec
    m_codecCtx->width = m_width;
    m_codecCtx->height = m_height;
    m_codecCtx->time_base = AVRational{ 1, m_fps };
    m_codecCtx->framerate = AVRational{ m_fps, 1 };
    m_codecCtx->pix_fmt = AV_PIX_FMT_NV12;
    m_codecCtx->bit_rate = m_bitrate;
    m_codecCtx->gop_size = m_fps * 2; // 2 second GOP

    // NVENC specific settings
    av_opt_set(m_codecCtx->priv_data, "preset", "p7", 0);      // Highest quality
    av_opt_set(m_codecCtx->priv_data, "tune", "hq", 0);        // High quality
    av_opt_set(m_codecCtx->priv_data, "profile", "high", 0);   // High profile
    av_opt_set(m_codecCtx->priv_data, "rc", "vbr", 0);         // Variable bitrate

    // Open codec
    int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Failed to open codec: " << errBuf << std::endl;
        return false;
    }

    // Allocate output format context
    avformat_alloc_output_context2(&m_formatCtx, nullptr, nullptr, m_outputPath.c_str());
    if (!m_formatCtx) {
        std::cerr << "Failed to allocate format context" << std::endl;
        return false;
    }

    // Create stream
    m_stream = avformat_new_stream(m_formatCtx, m_codec);
    if (!m_stream) {
        std::cerr << "Failed to create stream" << std::endl;
        return false;
    }

    m_stream->time_base = m_codecCtx->time_base;
    avcodec_parameters_from_context(m_stream->codecpar, m_codecCtx);

    // Open output file
    ret = avio_open(&m_formatCtx->pb, m_outputPath.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Failed to open output file: " << errBuf << std::endl;
        return false;
    }

    // Write header
    ret = avformat_write_header(m_formatCtx, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Failed to write header: " << errBuf << std::endl;
        return false;
    }

    // Initialize swscale for BGRA to NV12 conversion
    m_swsCtx = sws_getContext(
        m_width, m_height, AV_PIX_FMT_BGRA,
        m_width, m_height, AV_PIX_FMT_NV12,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!m_swsCtx) {
        std::cerr << "Failed to create swscale context" << std::endl;
        return false;
    }

    // Allocate frame
    m_frame = av_frame_alloc();
    if (!m_frame) {
        std::cerr << "Failed to allocate frame" << std::endl;
        return false;
    }

    m_frame->format = AV_PIX_FMT_NV12;
    m_frame->width = m_width;
    m_frame->height = m_height;

    ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
        std::cerr << "Failed to allocate frame buffer" << std::endl;
        return false;
    }

    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        std::cerr << "Failed to allocate packet" << std::endl;
        return false;
    }

    std::cout << "VideoEncoder initialized: " << m_outputPath << " ("
        << m_width << "x" << m_height << " @ " << m_fps << " fps)" << std::endl;
    return true;
}

bool VideoEncoder::EncodeFrame(const uint8_t* bgraData, int stride) {
    // Convert BGRA to NV12
    const uint8_t* srcData[1] = { bgraData };
    int srcLinesize[1] = { stride };

    sws_scale(m_swsCtx, srcData, srcLinesize, 0, m_height,
        m_frame->data, m_frame->linesize);

    // Set frame PTS
    m_frame->pts = m_frameCount++;

    // Send frame to encoder
    int ret = avcodec_send_frame(m_codecCtx, m_frame);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Failed to send frame: " << errBuf << std::endl;
        return false;
    }

    // Receive packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        else if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            std::cerr << "Failed to receive packet: " << errBuf << std::endl;
            return false;
        }

        if (!WritePacket(m_packet)) {
            return false;
        }
        av_packet_unref(m_packet);
    }

    return true;
}

bool VideoEncoder::WritePacket(AVPacket* packet) {
    av_packet_rescale_ts(packet, m_codecCtx->time_base, m_stream->time_base);
    packet->stream_index = m_stream->index;

    int ret = av_interleaved_write_frame(m_formatCtx, packet);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Failed to write frame: " << errBuf << std::endl;
        return false;
    }

    return true;
}

bool VideoEncoder::Finalize() {
    // Flush encoder
    avcodec_send_frame(m_codecCtx, nullptr);

    int ret;
    while (true) {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR_EOF) {
            break;
        }
        else if (ret < 0) {
            break;
        }

        WritePacket(m_packet);
        av_packet_unref(m_packet);
    }

    // Write trailer
    av_write_trailer(m_formatCtx);

    std::cout << "Encoded " << m_frameCount << " frames successfully" << std::endl;
    return true;
}
