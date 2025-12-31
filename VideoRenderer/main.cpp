#include "Application.h"
#include "Slide.h"

#include <iostream>
#include <string>
#include <fstream>


int main()
{
    std::cout << "=== VIDEO RENDERER ===\n\n";

    int n;
    std::cout << "Enter slide number: ";
    std::cin >> n;

    const uint16_t width = 3840;
    const uint16_t height = 2160;
    const uint8_t fps = 60;
    const uint16_t duration = 3;        // in seconds
    // const std::string output = "render/output.mp4";

    std::string output = "render/";
    output += std::to_string(n);
    output += ".mp4";

    Slide* pSlide = new Slide(n);

    Application app(width, height, fps, pSlide->m_Duration);
    if (!app.Initialize(output, pSlide))
    {
        std::cerr << "Failed to initialize application\n";
        return -1;
    }

    app.Run();

    std::cout << "\nVideo saved to: " << output << "\n";
    std::cout << "Press Enter to exit...\n";
    std::cin.get();

    delete pSlide;

    return 0;
}
