#include "Application.h"

#include <iostream>
#include <string>
#include <fstream>


void GetHeader(std::wstring& header)
{
    std::wstring line;
    std::wifstream inFile("../in/slideinfo/1.txt");

    if (!inFile)
    {
        std::cerr << "ERROR: Could not open slide info for reading.\n";
        return;
    }

    uint8_t i = 0;
    while (getline(inFile, line))
    {
        if (i == 0)
            header = line;
        i++;
    }

    inFile.close();
}

void GetCode(std::wstring& code, int n)
{
    std::wstring line;
    std::string fileName = "../in/code/";
    fileName += std::to_string(n);
    fileName += ".txt";
    std::cout << fileName << '\n';

    std::wifstream inFile(fileName);

    if (!inFile)
    {
        std::cerr << "ERROR: Could not open code for reading.\n";
        return;
    }

    uint8_t i = 0;
    while (getline(inFile, line))
    {
        if (i != 0)
            code += '\n';
        code += line;
        i++;
    }

    inFile.close();
}


int main()
{
    std::cout << "=== VIDEO RENDERER ===\n\n";

    int n;
    std::cout << "\n\nPlease enter slide number: ";
    std::cin >> n;

    const uint16_t width = 3840;
    const uint16_t height = 2160;
    const uint8_t fps = 10;
    const uint16_t duration = 3;        // in seconds
    const std::string output = "../bin/output.mp4";

    std::wstring header, code;
    GetHeader(header);
    GetCode(code, n);

    Application app(width, height, fps, duration);
    if (!app.Initialize(output, header, code))
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
