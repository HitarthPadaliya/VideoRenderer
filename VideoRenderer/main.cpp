#include "Application.h"
#include <iostream>

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Direct2D + FFmpeg NVENC Video Renderer" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Configuration
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    const int FPS = 60;
    const int DURATION_SECONDS = 10;
    const std::string OUTPUT_PATH = "output.mp4";

    // Create application
    Application app(WIDTH, HEIGHT, FPS, DURATION_SECONDS);

    // Initialize
    if (!app.Initialize(OUTPUT_PATH)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    // Run rendering loop
    app.Run();

    std::cout << "\nOutput saved to: " << OUTPUT_PATH << std::endl;
    std::cout << "Press Enter to exit...";
    std::cin.get();

    return 0;
}
