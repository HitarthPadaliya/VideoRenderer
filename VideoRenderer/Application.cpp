#include "Application.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Application::Application(int width, int height, int fps, int durationSeconds)
    : m_width(width)
    , m_height(height)
    , m_fps(fps)
    , m_totalFrames(fps* durationSeconds)
{
}

Application::~Application() {
}

bool Application::Initialize(const std::string& outputPath) {
    // Create renderer
    m_renderer = std::make_unique<Renderer>(m_width, m_height);
    if (!m_renderer->Initialize()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    // Create encoder
    m_encoder = std::make_unique<VideoEncoder>(outputPath, m_width, m_height, m_fps, 5000000);
    if (!m_encoder->Initialize()) {
        std::cerr << "Failed to initialize encoder" << std::endl;
        return false;
    }

    return true;
}

void Application::Run() {
    std::cout << "\nRendering " << m_totalFrames << " frames..." << std::endl;

    for (int frame = 0; frame < m_totalFrames; ++frame) {


        PixelBuffer buffer;
        if (m_renderer->LockPixelBuffer(buffer))
        {
            m_renderer->ClearBuffer(buffer, Color(255, 144, 0, 255));

            //BlurRectangle(
            //    buffer,
            //    50,
            //    50,
            //    450,
            //    200,
            //    8
            //);

            m_renderer->UnlockPixelBuffer();
        }

        // Render frame
        m_renderer->BeginFrame();
        RenderFrame(frame);
        m_renderer->EndFrame();

        // Get pixel data
        BYTE* pData = nullptr;
        UINT stride = 0;
        if (!m_renderer->GetPixelData(&pData, &stride)) {
            std::cerr << "Failed to get pixel data" << std::endl;
            break;
        }

        // Encode frame
        if (!m_encoder->EncodeFrame(pData, stride)) {
            std::cerr << "Failed to encode frame" << std::endl;
            m_renderer->ReleasePixelData();
            break;
        }

        m_renderer->ReleasePixelData();

        // Progress indicator
        if ((frame + 1) % 30 == 0 || frame == m_totalFrames - 1) {
            float progress = (frame + 1) * 100.0f / m_totalFrames;
            std::cout << "Progress: " << (frame + 1) << "/" << m_totalFrames
                << " (" << progress << "%)" << std::endl;
        }
    }

    // Finalize video
    m_encoder->Finalize();
    std::cout << "\nVideo rendering complete!" << std::endl;
}

void Application::RenderFrame(int frameNumber) {
    float t = static_cast<float>(frameNumber) / m_fps;
    float progress = static_cast<float>(frameNumber) / m_totalFrames;

    // Clear with animated background color
    float hue = progress * 360.0f;
    D2D1::ColorF bgColor(0.95f, 0.95f, 1.0f - progress * 0.2f);
    //m_renderer->Clear(bgColor);

    // Animated bouncing rectangle
    float rectX = 100 + std::sin(t * 2.0f) * 200.0f;
    float rectY = 100 + std::cos(t * 1.5f) * 150.0f;
    D2D1_RECT_F rect = D2D1::RectF(rectX, rectY, rectX + 200, rectY + 100);
    m_renderer->FillRectangle(rect, D2D1::ColorF(0.2f, 0.5f, 0.9f, 0.8f));
    m_renderer->DrawRectangle(rect, D2D1::ColorF(0.1f, 0.3f, 0.7f), 3.0f);

    // Animated rounded rectangle
    float roundX = 400 + std::cos(t * 1.8f) * 150.0f;
    float roundY = 300 + std::sin(t * 1.2f) * 100.0f;
    D2D1_RECT_F roundRect = D2D1::RectF(roundX, roundY, roundX + 250, roundY + 150);
    m_renderer->FillRoundedRectangle(roundRect, 30, 30, D2D1::ColorF(0.9f, 0.3f, 0.3f, 0.7f));

    // Rotating circles
    int numCircles = 8;
    for (int i = 0; i < numCircles; ++i) {
        float angle = t + (i * 2.0f * M_PI / numCircles);
        float cx = m_width / 2.0f + std::cos(angle) * 300.0f;
        float cy = m_height / 2.0f + std::sin(angle) * 200.0f;
        float radius = 30.0f + std::sin(t * 3.0f + i) * 15.0f;

        float hue = (i * 360.0f / numCircles + t * 50.0f);
        while (hue >= 360.0f) hue -= 360.0f;
        float r = 0.5f + 0.5f * std::sin(hue * M_PI / 180.0f);
        float g = 0.5f + 0.5f * std::sin((hue + 120.0f) * M_PI / 180.0f);
        float b = 0.5f + 0.5f * std::sin((hue + 240.0f) * M_PI / 180.0f);

        m_renderer->FillEllipse(D2D1::Point2F(cx, cy), radius, radius,
            D2D1::ColorF(r, g, b, 0.6f));
    }

    // Animated lines
    for (int i = 0; i < 5; ++i) {
        float y = 50.0f + i * 150.0f + std::sin(t * 2.0f + i) * 50.0f;
        m_renderer->DrawLine(
            D2D1::Point2F(50, y),
            D2D1::Point2F(m_width - 50.0f, y),
            D2D1::ColorF(0.3f, 0.7f, 0.5f, 0.5f),
            2.0f + i
        );
    }

    // Title text with ligatures
    D2D1_RECT_F titleRect = D2D1::RectF(50, 30, m_width - 50.0f, 120);
    m_renderer->DrawText(
        L"Direct2D -> FFmpeg -> NVENC",
        titleRect,
        D2D1::ColorF(0.1f, 0.1f, 0.3f),
        L"Cascadia Code",
        48.0f,
        DWRITE_FONT_WEIGHT_BOLD
    );

    // Frame counter with ligatures
    std::wstring frameText = L"Frame: " + std::to_wstring(frameNumber) +
        L" / " + std::to_wstring(m_totalFrames);
    frameText += L" | Time: " + std::to_wstring(static_cast<int>(t)) + L"s";
    D2D1_RECT_F counterRect = D2D1::RectF(50, m_height - 100.0f, m_width - 50.0f, m_height - 20.0f);
    m_renderer->DrawText(
        frameText,
        counterRect,
        D2D1::ColorF(0.2f, 0.2f, 0.2f),
        L"Consolas",
        32.0f
    );

    // Code sample with ligatures
    D2D1_RECT_F codeRect = D2D1::RectF(50, m_height - 250.0f, m_width - 50.0f, m_height - 120.0f);
    m_renderer->DrawText(
        L"if (x != y && a >= b) { /* -> <= >= != */ }",
        codeRect,
        D2D1::ColorF(0.3f, 0.5f, 0.7f),
        L"Fira Code",
        24.0f
    );
}
