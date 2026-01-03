#pragma once

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <string>
#include <string_view>
#include <cwctype>


class Slide
{
	public:
        std::wstring m_Header;
        std::wstring m_Code;
        bool m_bOpenWindow      = false;
        bool m_bCloseWindow     = false;
        float m_Duration        = 0.0f;
        float m_CodeDuration    = 0.0f;
        int m_SlideNo           = 1;
        int m_BGNo              = 1;
        float m_FontSize        = 72.0f;

        std::unordered_set<std::wstring> m_Classes;
        std::unordered_set<std::wstring> m_Macros;
        std::unordered_set<std::wstring> m_Functions;
        std::unordered_set<std::wstring> m_Params;
        std::unordered_set<std::wstring> m_LocalVars;
        std::unordered_set<std::wstring> m_MemberVars;


    public:
        Slide(const int& n);


    private:
        static std::wstring AfterPrefix(const std::wstring& s, const std::wstring& prefix);
        static std::wstring_view Trim(std::wstring_view v);

        void ParseNames(std::wstring_view line, std::unordered_set<std::wstring>& arr);
};
