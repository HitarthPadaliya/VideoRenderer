#include "VideoEncoder.h"

#include <iostream>
#include <sstream>
#include <vector>

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

VideoEncoder::VideoEncoder(const std::string& outputPath, int width, int height, int fps, int bitrate)
    : m_outputPath(outputPath),
    m_width(width),
    m_height(height),
    m_fps(fps),
    m_bitrate(bitrate) {
    ZeroMemory(&m_pi, sizeof(m_pi));
}

VideoEncoder::~VideoEncoder() {
    Finalize();
    CloseHandles();
}

void VideoEncoder::CloseHandles() {
    if (m_hChildStdinWrite) {
        CloseHandle(m_hChildStdinWrite);
        m_hChildStdinWrite = nullptr;
    }
    if (m_hChildStdinRead) {
        CloseHandle(m_hChildStdinRead);
        m_hChildStdinRead = nullptr;
    }
    if (m_pi.hThread) {
        CloseHandle(m_pi.hThread);
        m_pi.hThread = nullptr;
    }
    if (m_pi.hProcess) {
        CloseHandle(m_pi.hProcess);
        m_pi.hProcess = nullptr;
    }
}

bool VideoEncoder::Initialize() {
    if (m_initialized) return true;

    if (!StartFFmpegProcess()) {
        std::cerr << "Failed to start ffmpeg process.\n";
        return false;
    }

    m_initialized = true;

    std::cout
        << "VideoEncoder initialized (external ffmpeg)\n"
        << "  Output: " << m_outputPath << "\n"
        << "  Input:  rgbaf16le " << m_width << "x" << m_height << " @" << m_fps << "fps\n"
        << "  Encode: hevc_nvenc main10 (CQ=" << m_cq << ")\n";

    return true;
}

bool VideoEncoder::StartFFmpegProcess() {
    // Create a pipe for the child process's STDIN.
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&m_hChildStdinRead, &m_hChildStdinWrite, &sa, 0)) {
        std::cerr << "CreatePipe failed: " << GetLastError() << "\n";
        return false;
    }

    // Ensure the write handle is NOT inherited.
    if (!SetHandleInformation(m_hChildStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "SetHandleInformation failed: " << GetLastError() << "\n";
        return false;
    }

    // Build filter chain:
    // - deband (optional) to reduce contouring
    // - subtle temporal noise to hide remaining steps (helps gradients post-compression)
    // - zscale with error diffusion dithering during quantization
    // - convert to p010le for main10
    std::ostringstream vf;
    if (m_enableDeband) {
        vf << "deband=1thr=0.02:2thr=0.02:3thr=0.02:range=16:blur=1,";
    }
    vf << "noise=alls=" << m_noiseStrength << ":allf=t+u,"
        << "zscale=dither=error_diffusion,"
        << "format=p010le";

    // IMPORTANT:
    // - We feed rawvideo on stdin as rgbaf16le (packed RGBA half-float).
    // - ffmpeg will do conversion + dithering + encode via hevc_nvenc main10.
    std::ostringstream cmd;
    cmd
        << "ffmpeg -y "
        << "-hide_banner "
        << "-loglevel info "
        << "-f rawvideo "
        << "-pix_fmt rgbaf16le "
        << "-s " << m_width << "x" << m_height << " "
        << "-r " << m_fps << " "
        << "-i - "
        << "-vf \"" << vf.str() << "\" "
        << "-c:v hevc_nvenc "
        << "-profile:v main10 "
        << "-preset p7 "
        << "-tune hq "
        << "-rc vbr "
        << "-cq " << m_cq << " "
        << "-rc-lookahead " << m_lookahead << " "
        << "-pix_fmt p010le "
        << "\"" << m_outputPath << "\"";

    std::wstring wcmd = Utf8ToWide(cmd.str());

    STARTUPINFOW si{};
    si.cb = sizeof(si);

    // Redirect STDIN for the child.
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = m_hChildStdinRead;

    // Inherit parent's stdout/stderr so you can see ffmpeg logs in console.
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    DWORD creationFlags = CREATE_NO_WINDOW; // comment this out if you want a visible console window

    // CreateProcess requires a mutable buffer for the command line.
    std::vector<wchar_t> mutableCmd(wcmd.begin(), wcmd.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(
        nullptr,
        mutableCmd.data(),
        nullptr,
        nullptr,
        TRUE, // inherit handles
        creationFlags,
        nullptr,
        nullptr,
        &si,
        &m_pi)) {
        std::cerr << "CreateProcessW failed: " << GetLastError() << "\n";
        return false;
    }

    // The child has its own handle now; parent can close the read end.
    CloseHandle(m_hChildStdinRead);
    m_hChildStdinRead = nullptr;

    return true;
}

bool VideoEncoder::EncodeFrame(const uint8_t* rgbaF16Data, int strideBytes) {
    if (!m_initialized) return false;

    const int expectedStride = m_width * 8; // 4 channels * 16-bit half = 8 bytes/pixel
    if (strideBytes != expectedStride) {
        std::cerr << "EncodeFrame: unexpected stride. Expected " << expectedStride
            << " got " << strideBytes << "\n";
        return false;
    }

    const size_t frameBytes = static_cast<size_t>(strideBytes) * static_cast<size_t>(m_height);

    DWORD written = 0;
    BOOL ok = WriteFile(m_hChildStdinWrite, rgbaF16Data, (DWORD)frameBytes, &written, nullptr);
    if (!ok || written != frameBytes) {
        std::cerr << "WriteFile to ffmpeg stdin failed. err=" << GetLastError()
            << " written=" << written << " expected=" << frameBytes << "\n";
        return false;
    }

    return true;
}

bool VideoEncoder::Finalize() {
    if (!m_initialized) return true;

    // Close stdin to signal EOF to ffmpeg.
    if (m_hChildStdinWrite) {
        CloseHandle(m_hChildStdinWrite);
        m_hChildStdinWrite = nullptr;
    }

    // Wait for ffmpeg to exit.
    if (m_pi.hProcess) {
        WaitForSingleObject(m_pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        if (GetExitCodeProcess(m_pi.hProcess, &exitCode)) {
            if (exitCode != 0) {
                std::cerr << "ffmpeg exited with code " << exitCode << "\n";
                // still mark as finalized; caller can inspect logs
            }
        }
    }

    m_initialized = false;
    return true;
}
