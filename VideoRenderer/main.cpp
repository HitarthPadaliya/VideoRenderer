#include "Application.h"

#include <iostream>
#include <string>


int main()
{
    std::cout << "=== VIDEO RENDERER ===\n\n";

    const uint16_t width = 1920;
    const uint16_t height = 1080;
    const uint8_t fps = 60;
    const uint16_t duration = 5;        // in seconds
    const std::string output = "../bin/output.mp4";

    Application app(width, height, fps, duration);
    if (!app.Initialize(output))
    {
        std::cerr << "Failed to initialize application\n";
        return -1;
    }

    app.Run();

    std::cout << "\nVideo saved to: " << output << "\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
    return 0;
}
