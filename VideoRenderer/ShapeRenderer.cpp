#include "ShapeRenderer.h"
#include <cstring>

ShapeRenderer::ShapeRenderer(uint8_t* pixels, uint32_t width, uint32_t height, uint32_t stride)
    : m_pixels(pixels), m_width(width), m_height(height), m_stride(stride) {
}

uint8_t* ShapeRenderer::GetPixelPtr(int x, int y) {
    if (x < 0 || x >= static_cast<int>(m_width) || y < 0 || y >= static_cast<int>(m_height))
        return nullptr;
    return m_pixels + y * m_stride + x * 4;
}

void ShapeRenderer::SetPixel(int x, int y, const Color& color) {
    uint8_t* pixel = GetPixelPtr(x, y);
    if (pixel) {
        *reinterpret_cast<uint32_t*>(pixel) = *reinterpret_cast<const uint32_t*>(&color);
    }
}

void ShapeRenderer::BlendPixel(int x, int y, const Color& color) {
    uint8_t* pixel = GetPixelPtr(x, y);
    if (!pixel) return;

    float alpha = color.a / 255.0f;
    float invAlpha = 1.0f - alpha;

    pixel[0] = static_cast<uint8_t>(color.b * alpha + pixel[0] * invAlpha);
    pixel[1] = static_cast<uint8_t>(color.g * alpha + pixel[1] * invAlpha);
    pixel[2] = static_cast<uint8_t>(color.r * alpha + pixel[2] * invAlpha);
    pixel[3] = std::min(255, static_cast<int>(color.a + pixel[3] * invAlpha));
}

void ShapeRenderer::Clear(const Color& color) {
    for (uint32_t y = 0; y < m_height; ++y) {
        uint32_t* row = reinterpret_cast<uint32_t*>(m_pixels + y * m_stride);
        uint32_t colorValue = *reinterpret_cast<const uint32_t*>(&color);
        for (uint32_t x = 0; x < m_width; ++x) {
            row[x] = colorValue;
        }
    }
}

void ShapeRenderer::FillRectangle(float x, float y, float width, float height, const Color& color) {
    int x1 = static_cast<int>(x);
    int y1 = static_cast<int>(y);
    int x2 = static_cast<int>(x + width);
    int y2 = static_cast<int>(y + height);

    x1 = std::max(0, std::min(x1, static_cast<int>(m_width)));
    y1 = std::max(0, std::min(y1, static_cast<int>(m_height)));
    x2 = std::max(0, std::min(x2, static_cast<int>(m_width)));
    y2 = std::max(0, std::min(y2, static_cast<int>(m_height)));

    for (int py = y1; py < y2; ++py) {
        uint32_t* row = reinterpret_cast<uint32_t*>(m_pixels + py * m_stride);
        uint32_t colorValue = *reinterpret_cast<const uint32_t*>(&color);
        for (int px = x1; px < x2; ++px) {
            row[px] = colorValue;
        }
    }
}

void ShapeRenderer::FillRoundedRectangle(float x, float y, float width, float height,
    float radiusX, float radiusY, const Color& color) {
    int x1 = static_cast<int>(x);
    int y1 = static_cast<int>(y);
    int x2 = static_cast<int>(x + width);
    int y2 = static_cast<int>(y + height);

    radiusX = std::min(radiusX, width * 0.5f);
    radiusY = std::min(radiusY, height * 0.5f);

    for (int py = y1; py < y2; ++py) {
        for (int px = x1; px < x2; ++px) {
            float dx = 0, dy = 0;

            if (px < x + radiusX) dx = (x + radiusX - px) / radiusX;
            else if (px > x + width - radiusX) dx = (px - (x + width - radiusX)) / radiusX;

            if (py < y + radiusY) dy = (y + radiusY - py) / radiusY;
            else if (py > y + height - radiusY) dy = (py - (y + height - radiusY)) / radiusY;

            float dist = dx * dx + dy * dy;
            if (dist <= 1.0f) {
                SetPixel(px, py, color);
            }
        }
    }
}

void ShapeRenderer::FillEllipse(float centerX, float centerY, float radiusX, float radiusY,
    const Color& color) {
    int x1 = static_cast<int>(centerX - radiusX);
    int y1 = static_cast<int>(centerY - radiusY);
    int x2 = static_cast<int>(centerX + radiusX);
    int y2 = static_cast<int>(centerY + radiusY);

    for (int py = y1; py <= y2; ++py) {
        for (int px = x1; px <= x2; ++px) {
            float dx = (px - centerX) / radiusX;
            float dy = (py - centerY) / radiusY;
            if (dx * dx + dy * dy <= 1.0f) {
                SetPixel(px, py, color);
            }
        }
    }
}

void ShapeRenderer::DrawRectangle(float x, float y, float width, float height,
    const Color& color, float strokeWidth) {
    FillRectangle(x, y, width, strokeWidth, color);
    FillRectangle(x, y + height - strokeWidth, width, strokeWidth, color);
    FillRectangle(x, y, strokeWidth, height, color);
    FillRectangle(x + width - strokeWidth, y, strokeWidth, height, color);
}

void ShapeRenderer::DrawEllipse(float centerX, float centerY, float radiusX, float radiusY,
    const Color& color, float strokeWidth) {
    int x1 = static_cast<int>(centerX - radiusX - strokeWidth);
    int y1 = static_cast<int>(centerY - radiusY - strokeWidth);
    int x2 = static_cast<int>(centerX + radiusX + strokeWidth);
    int y2 = static_cast<int>(centerY + radiusY + strokeWidth);

    for (int py = y1; py <= y2; ++py) {
        for (int px = x1; px <= x2; ++px) {
            float dx = (px - centerX) / radiusX;
            float dy = (py - centerY) / radiusY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist >= 1.0f && dist <= 1.0f + strokeWidth / radiusX) {
                SetPixel(px, py, color);
            }
        }
    }
}
