#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

struct Color {
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
};

class ShapeRenderer {
public:
    ShapeRenderer(uint8_t* pixels, uint32_t width, uint32_t height, uint32_t stride);

    void Clear(const Color& color);
    void FillRectangle(float x, float y, float width, float height, const Color& color);
    void FillRoundedRectangle(float x, float y, float width, float height,
        float radiusX, float radiusY, const Color& color);
    void FillEllipse(float centerX, float centerY, float radiusX, float radiusY,
        const Color& color);
    void DrawRectangle(float x, float y, float width, float height,
        const Color& color, float strokeWidth = 1.0f);
    void DrawEllipse(float centerX, float centerY, float radiusX, float radiusY,
        const Color& color, float strokeWidth = 1.0f);

private:
    void SetPixel(int x, int y, const Color& color);
    void BlendPixel(int x, int y, const Color& color);
    uint8_t* GetPixelPtr(int x, int y);

    uint8_t* m_pixels;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_stride;
};
