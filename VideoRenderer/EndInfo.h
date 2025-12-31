#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <d2d1_1.h>


class EndInfo
{
	public:
		float m_WindowX;
		float m_WindowY;
		float m_HeaderY;


	public:
		EndInfo(const int& n);
		EndInfo(const int& n, const D2D1_POINT_2F& windowSize, const float& headerY);


	private:
		static std::wstring AfterPrefix(const std::wstring& s, const std::wstring& prefix);
};
