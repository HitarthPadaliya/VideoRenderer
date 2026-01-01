#include "Application.h"

#include <iostream>
#include <sstream>
#include <chrono>


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
    , m_TotalFrames((uint32_t)fps * duration)
{}

Application::~Application() {}


bool Application::Initialize(const std::string& outputPath, Slide* pSlide)
{
    m_pRenderer = std::make_unique<Renderer>(m_Width, m_Height);
    if (!m_pRenderer->Initialize(pSlide))
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
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    std::cout << "\nRendering " << m_TotalFrames << " frames...\n";
    for (int frame = 1; frame <= m_TotalFrames; ++frame)
    {
        float t = static_cast<float>(frame) / static_cast<float>(m_FPS);
        float p = static_cast<float>(frame) / static_cast<float>(m_TotalFrames);

        m_pRenderer->RenderCompute(t, p);

        m_pRenderer->BeginFrame();
        RenderOverlay(frame + 1);
        m_pRenderer->EndFrame();

        if (!m_pEncoder->EncodeFrame(m_pRenderer->GetRenderTexture()))
        {
            std::cerr << "Failed to encode frame " << frame << "\n";
            break;
        }

        if (frame % 30 != 0)
            continue;

        uint8_t percent = (float)frame / m_TotalFrames * 100;
        
        std::cout << "\r";
        std::cout << frame << '/' << m_TotalFrames << ' ' << (int)percent << "%  \t[";
        for (uint8_t p = 1; p <= percent; ++p)
            std::cout << (char)254u;
        for (uint8_t p = percent; p < 100; ++p)
            std::cout << ' ';
        std::cout << "]";
    }

    std::cout << '\n';
    m_pEncoder->Finalize();
    std::cout << "\nVideo rendering complete!\n";

    auto t1 = clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "Total render time: " << seconds << " s\n";
}


void Application::RenderOverlay(const uint32_t& frameNumber)
{
    m_pRenderer->DrawHeader();
    m_pRenderer->DrawCode();
}
