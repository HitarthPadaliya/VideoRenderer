#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <d2d1_1.h>

class Color {

public:
    uint8_t b, g, r, a;  // BGRA format for WIC

    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : b(blue), g(green), r(red), a(alpha) {
    }

    static Color FromFloat(float red, float green, float blue, float alpha = 1.0f) {
        return Color(
            static_cast<uint8_t>(red * 255),
            static_cast<uint8_t>(green * 255),
            static_cast<uint8_t>(blue * 255),
            static_cast<uint8_t>(alpha * 255)
        );
    }

    D2D1::ColorF GetD2DColor()
    {
        return D2D1::ColorF(r, g, b, a);
    }

    inline uint32_t PackBGRA()
    {
        return
            (uint32_t(a) << 24) |
            (uint32_t(r) << 16) |
            (uint32_t(g) << 8) |
            uint32_t(b);
    }
};

struct PixelBuffer {
    uint8_t* Data;
    UINT Width;
    UINT Height;
    UINT Stride;
};


class ShapeRenderer
{
    public:
        uint8_t* m_Data;
        UINT m_Width;
        UINT m_Height;
        UINT m_Stride;

        void SetPixel(UINT x, UINT y, Color col);
        void Clear(Color col);
};
