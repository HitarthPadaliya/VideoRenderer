#pragma once

#include <string>
#include <windows.h>

class VideoEncoder {
public:
    // bitrate is kept for API compatibility; this implementation uses CQ-based VBR by default.
    VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate);

    ~VideoEncoder();

    bool Initialize();
    bool EncodeFrame(const uint8_t* rgbaF16Data, int strideBytes);
    bool Finalize();

private:
    bool StartFFmpegProcess();
    void CloseHandles();

private:
    std::string m_outputPath;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_bitrate = 0;

    // Quality controls (reasonable defaults for gradients).
    int m_cq = 18;                 // lower = better quality/larger file
    int m_lookahead = 32;
    int m_noiseStrength = 2;       // subtle temporal noise to reduce banding
    bool m_enableDeband = true;

    // FFmpeg process plumbing
    PROCESS_INFORMATION m_pi{};
    HANDLE m_hChildStdinWrite = nullptr;
    HANDLE m_hChildStdinRead = nullptr;

    bool m_initialized = false;
};
