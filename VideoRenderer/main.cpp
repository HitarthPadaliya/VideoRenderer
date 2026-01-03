#include "Application.h"
#include "Slide.h"

#include <iostream>
#include <string>
#include <fstream>


int main()
{
    const uint16_t width = 3840;
    const uint16_t height = 2160;
    const uint8_t fps = 60;

    int n;
    do
    {
        system("cls");

        std::cout << "=== VIDEO RENDERER ===\n\n";
        std::cout << "Enter slide number: ";
        std::cin >> n;

        if (n <= 0)
            continue;

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

        std::cout << "\nVideo saved to: " << output << "\n\n";

        delete pSlide;
    }
    while (n > 0);

    std::cout << "Exiting...";

    return 0;
}
