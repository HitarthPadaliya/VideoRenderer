#pragma once

#include "Renderer.h"
#include "VideoEncoder.h"

#include <memory>
#include <string>

class Application {
public:
    Application(int width, int height, int fps, int durationSeconds);
    ~Application();

    bool Initialize(const std::string& outputPath);
    void Run();

private:
    void RenderOverlay(int frameNumber);

private:
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    int m_totalFrames = 0;

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<VideoEncoder> m_encoder;
};
