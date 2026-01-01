#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <string_view>

class Slide
{
public:
    std::wstring m_Header;
    std::wstring m_Code;

    bool  m_bOpenWindow  = false;
    bool  m_bCloseWindow = false;

    float m_Duration     = 0.0f;
    float m_CodeDuration = 0.0f;

    float m_WindowInDuration    = 0.5f;  // seconds
    float m_WindowOutDuration   = 0.5f;  // seconds
    float m_CodeRevealStartTime = 0.5f;  // seconds from slide start
    float m_CodeRevealFadeTime  = 1.0f;  // seconds for full reveal

    int m_SlideNo = 1;

    std::unordered_set<std::wstring> m_Classes;
    std::unordered_set<std::wstring> m_Macros;
    std::unordered_set<std::wstring> m_Functions;
    std::unordered_set<std::wstring> m_Params;
    std::unordered_set<std::wstring> m_LocalVars;
    std::unordered_set<std::wstring> m_MemberVars;

public:
    Slide(const int& n);

private:
    static std::wstring      AfterPrefix(const std::wstring& s, const std::wstring& prefix);
    static std::wstring_view Trim(std::wstring_view v);
    void                     ParseNames(std::wstring_view line, std::unordered_set<std::wstring>& arr);
};
