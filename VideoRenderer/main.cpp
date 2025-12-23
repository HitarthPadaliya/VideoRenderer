#include "Application.h"

#include <iostream>
#include <string>

int main() {
    std::cout << "Compute -> Direct2D -> FFmpeg -> HEVC NVENC Main10\n\n";

    const int WIDTH = 640;
    const int HEIGHT = 360;
    const int FPS = 60;
    const int DURATION_SECONDS = 10;
    const std::string OUTPUT_PATH = "output_hevc_main10.mp4";

    Application app(WIDTH, HEIGHT, FPS, DURATION_SECONDS);
    if (!app.Initialize(OUTPUT_PATH)) {
        std::cerr << "Failed to initialize application\n";
        return 1;
    }

    app.Run();

    std::cout << "\nVideo saved to: " << OUTPUT_PATH << "\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
    return 0;
}
