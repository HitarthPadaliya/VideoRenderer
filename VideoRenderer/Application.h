#pragma once

#include "Renderer.h"
#include "VideoEncoder.h"
#include <memory>

class Application {
public:
    Application(int width, int height, int fps, int durationSeconds);
    ~Application();

    bool Initialize(const std::string& outputPath);
    void Run();

private:
    void RenderFrame(int frameNumber);

    int m_width;
    int m_height;
    int m_fps;
    int m_totalFrames;

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<VideoEncoder> m_encoder;
};
