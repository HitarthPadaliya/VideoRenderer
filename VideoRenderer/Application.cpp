#include "Application.h"

#include <iostream>
#include <sstream>

Application::Application(int width, int height, int fps, int durationSeconds)
    : m_width(width),
    m_height(height),
    m_fps(fps),
    m_totalFrames(fps* durationSeconds) {
}

Application::~Application() {}

bool Application::Initialize(const std::string& outputPath) {
    m_renderer = std::make_unique<Renderer>(m_width, m_height);
    if (!m_renderer->Initialize()) {
        std::cerr << "Failed to initialize renderer\n";
        return false;
    }

    m_encoder = std::make_unique<VideoEncoder>(outputPath, m_width, m_height, m_fps, 0);

    // Pass D3D11 device to encoder for GPU-to-GPU workflow
    if (!m_encoder->Initialize(m_renderer->GetDevice())) {
        std::cerr << "Failed to initialize encoder\n";
        return false;
    }

    return true;
}

void Application::Run() {
    std::cout << "\nRendering " << m_totalFrames << " frames...\n";
    for (int frame = 0; frame < m_totalFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(m_fps);
        float p = static_cast<float>(frame) / static_cast<float>(m_totalFrames);

        // 1) Compute shader draws shapes into RGBA16F texture
        m_renderer->RenderCompute(t, p);

        // 2) Direct2D draws text on top
        m_renderer->BeginFrame();
        RenderOverlay(frame);
        m_renderer->EndFrame();

        // 3) Encode directly from GPU texture (NO CPU READBACK!)
        if (!m_encoder->EncodeFrame(m_renderer->GetRenderTexture())) {
            std::cerr << "Failed to encode frame " << frame << "\n";
            break;
        }

        if ((frame + 1) % 30 == 0 || frame == m_totalFrames - 1) {
            float progress = (frame + 1) * 100.0f / m_totalFrames;
            std::cout << "Progress: " << (frame + 1) << "/" << m_totalFrames
                << " (" << progress << "%)\n";
        }
    }

    m_encoder->Finalize();
    std::cout << "\nVideo rendering complete!\n";
}

void Application::RenderOverlay(int frameNumber) {
    D2D1_RECT_F titleRect = D2D1::RectF(50.0f, 30.0f, (float)m_width - 50.0f, 120.0f);
    m_renderer->DrawText(
        L"Compute Shader (RGBA16F) -> Direct2D text -> libavcodec NVENC -> HEVC Main10",
        titleRect,
        D2D1::ColorF(0.95f, 0.95f, 0.98f, 1.0f),
        L"Consolas ligaturized v3",
        32.0f,
        DWRITE_FONT_WEIGHT_BOLD
    );

    std::wstring frameText =
        L"Frame: " + std::to_wstring(frameNumber) +
        L" / " + std::to_wstring(m_totalFrames);

    D2D1_RECT_F counterRect =
        D2D1::RectF(50.0f, (float)m_height - 80.0f,
            (float)m_width - 50.0f, (float)m_height - 20.0f);

    m_renderer->DrawText(
        frameText,
        counterRect,
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),
        L"Consolas ligaturized v3",
        28.0f
    );
}
