#include "Slide.h"


Slide::Slide(const int& n)
{
    m_SlideNo = n;

	std::wstring line;
    std::string fileName = "../in/slideinfo/";
    fileName += std::to_string(n);
    fileName += ".txt";

    std::wifstream slideFile(fileName);
    if (!slideFile)
    {
        std::cerr << "ERROR: Could not open slide info for reading.\n";
        return;
    }

    while (getline(slideFile, line))
    {
        if (line.starts_with(L"Header = "))
            m_Header = AfterPrefix(line, L"Header = ");
        else if (line.starts_with(L"Duration = "))
            m_Duration = _wtof(AfterPrefix(line, L"Duration = ").c_str());
        else if (line.starts_with(L"CodeDuration = "))
            m_CodeDuration = _wtof(AfterPrefix(line, L"CodeDuration = ").c_str());
        else if (line.starts_with(L"Open"))
            m_bOpenWindow = true;
        else if (line.starts_with(L"Close"))
            m_bCloseWindow = true;
        else if (line.starts_with(L"Classes = "))
            ParseNames(AfterPrefix(line, L"Classes = "), m_Classes);
        else if (line.starts_with(L"Macros = "))
            ParseNames(AfterPrefix(line, L"Macros = "), m_Macros);
        else if (line.starts_with(L"Functions = "))
            ParseNames(AfterPrefix(line, L"Functions = "), m_Functions);
        else if (line.starts_with(L"Params = "))
            ParseNames(AfterPrefix(line, L"Params = "), m_Params);
        else if (line.starts_with(L"LocalVars = "))
            ParseNames(AfterPrefix(line, L"LocalVars = "), m_LocalVars);
        else if (line.starts_with(L"MemberVars = "))
            ParseNames(AfterPrefix(line, L"MemberVars = "), m_MemberVars);
    }

    slideFile.close();

    fileName = "../in/code/";
    fileName += std::to_string(n);
    fileName += ".txt";

    std::wifstream codeFile(fileName);
    if (!codeFile)
    {
        std::cerr << "ERROR: Could not open code for reading.\n";
        return;
    }

    uint32_t i = 0;
    while (getline(codeFile, line))
    {
        if (i != 0)
            m_Code += '\n';
        m_Code += line;
        i++;
    }

    codeFile.close();
}


std::wstring Slide::AfterPrefix(const std::wstring& s, const std::wstring& prefix)
{
    return s.substr(prefix.size());
}

std::wstring_view Slide::Trim(std::wstring_view v)
{
    while (!v.empty() && std::iswspace(v.front()))
        v.remove_prefix(1);
    while (!v.empty() && std::iswspace(v.back()))
        v.remove_suffix(1);
    return v;
}


void Slide::ParseNames(std::wstring_view line, std::unordered_set<std::wstring>& arr)
{
    while (true)
    {
        const auto comma = line.find(L',');
        auto token = Trim(comma == std::wstring_view::npos ? line : line.substr(0, comma));
        if (!token.empty())
            arr.insert(std::wstring(token));

        if (comma == std::wstring_view::npos)
            break;
        line.remove_prefix(comma + 1);
    }
}
