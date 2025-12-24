#include "Application.h"

#include <iostream>
#include <sstream>

Application::Application
(
    const uint16_t& width,
    const uint16_t& height,
    const uint8_t& fps,
    const uint16_t& duration
)
    : m_Width(width)
    , m_Height(height)
    , m_FPS(fps)
    , m_TotalFrames(fps * duration)
{}

Application::~Application() {}


bool Application::Initialize(const std::string& outputPath)
{
    m_pRenderer = std::make_unique<Renderer>(m_Width, m_Height);
    if (!m_pRenderer->Initialize())
    {
        std::cerr << "Failed to initialize renderer\n";
        return false;
    }

    m_pEncoder = std::make_unique<VideoEncoder>(outputPath, m_Width, m_Height, m_FPS, 0);
    if (!m_pEncoder->Initialize(m_pRenderer->GetDevice()))
    {
        std::cerr << "Failed to initialize encoder\n";
        return false;
    }

    return true;
}

void Application::Run()
{
    std::cout << "\nRendering " << m_TotalFrames << " frames...\n";
    for (int frame = 1; frame <= m_TotalFrames; ++frame)
    {
        float t = static_cast<float>(frame) / static_cast<float>(m_FPS);
        float p = static_cast<float>(frame) / static_cast<float>(m_TotalFrames);

        m_pRenderer->RenderCompute(t, p);

        m_pRenderer->BeginFrame();
        RenderOverlay(frame);
        m_pRenderer->EndFrame();

        if (!m_pEncoder->EncodeFrame(m_pRenderer->GetRenderTexture()))
        {
            std::cerr << "Failed to encode frame " << frame << "\n";
            break;
        }

        uint8_t percent = (float)frame / m_TotalFrames * 100;
        if (percent != m_PrevPercent)
        {
            std::cout << "\r";
            std::cout << frame << '/' << m_TotalFrames << ' ' << (int)percent << "%\t[";
            for (uint8_t p = 1; p <= percent; ++p)
                std::cout << (char)254u;
            for (uint8_t p = percent; p < 100; ++p)
                std::cout << ' ';
            std::cout << "]";

            m_PrevPercent = percent;
        }
    }

    std::cout << '\n';
    m_pEncoder->Finalize();
    std::cout << "\nVideo rendering complete!\n";
}


void Application::RenderOverlay(const uint32_t& frameNumber)
{
    D2D1_RECT_F titleRect = D2D1::RectF(50.0f, 30.0f, (float)m_Width - 50.0f, 120.0f);
    m_pRenderer->DrawText
    (
        L"Compute Shader (RGBA16F) -> Direct2D text -> libavcodec NVENC -> HEVC Main10",
        titleRect,
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),
        L"Consolas ligaturized v3",
        32.0f,
        DWRITE_FONT_WEIGHT_BOLD
    );

    std::wstring frameText
        = L"Frame: " + std::to_wstring(frameNumber) + L" / " + std::to_wstring(m_TotalFrames);
    D2D1_RECT_F counterRect
        = D2D1::RectF(50.0f, (float)m_Height - 80.0f, (float)m_Width - 50.0f, (float)m_Height - 20.0f);

    m_pRenderer->DrawText
    (
        frameText,
        counterRect,
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),
        L"Consolas ligaturized v3",
        28.0f
    );
}
