#pragma once

#include "Renderer.h"
#include "VideoEncoder.h"
#include "Slide.h"

#include <memory>
#include <string>


class Application
{
    private:
        uint16_t m_Width = 0;
        uint16_t m_Height = 0;
        uint8_t m_FPS = 0;
        uint32_t m_TotalFrames = 0;

        uint8_t m_PrevPercent = 0;

        std::unique_ptr<Renderer> m_pRenderer;
        std::unique_ptr<VideoEncoder> m_pEncoder;


    public:
        Application(const uint16_t& width, const uint16_t& height, const uint8_t& fps, const uint16_t& duration);
        ~Application();
    
        bool Initialize(const std::string& outputPath, Slide* pSlide);
        void Run();
    

    private:
        void RenderOverlay(const uint32_t& frameNumber);
};
