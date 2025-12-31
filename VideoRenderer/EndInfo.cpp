#include "EndInfo.h"


EndInfo::EndInfo(const int& n)
{
    std::wstring line;
    std::string fileName = "../in/endinfo/";
    fileName += std::to_string(n);
    fileName += ".txt";

    std::wifstream inFile(fileName);
    if (!inFile)
    {
        std::cerr << "ERROR: Could not open end info for reading.\n";
        return;
    }

    while (getline(inFile, line))
    {
        if (line.starts_with(L"WindowX = "))
            m_WindowX = _wtof(AfterPrefix(line, L"WindowX = ").c_str());
        else if (line.starts_with(L"WindowY = "))
            m_WindowY = _wtof(AfterPrefix(line, L"WindowY = ").c_str());
        else if (line.starts_with(L"HeaderY = "))
            m_HeaderY = _wtof(AfterPrefix(line, L"HeaderY = ").c_str());
    }

    inFile.close();
}

EndInfo::EndInfo(const int& n, const D2D1_POINT_2F& windowSize, const float& headerY)
    : m_WindowX(windowSize.x)
    , m_WindowY(windowSize.y)
    , m_HeaderY(headerY)
{
    std::wstring line;
    std::string fileName = "../in/endinfo/";
    fileName += std::to_string(n);
    fileName += ".txt";

    std::wofstream outFile(fileName);
    if (!outFile.is_open())
    {
        std::cerr << "ERROR: Could not write end info.\n";
        return;
    }

    outFile << L"WindowX = " << m_WindowX << L"\nWindowY = " << m_WindowY << L"\nHeaderY = " << m_HeaderY;
    outFile.close();
}


std::wstring EndInfo::AfterPrefix(const std::wstring& s, const std::wstring& prefix)
{
    return s.substr(prefix.size());
}
