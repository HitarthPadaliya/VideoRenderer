#include "ShapeRenderer.h"


void ShapeRenderer::SetPixel(UINT x, UINT y, Color col)
{
    if (x >= m_Width || y >= m_Height)
        return;

    uint8_t* row = m_Data + y * m_Stride;
    uint32_t* pixel = reinterpret_cast<uint32_t*>(row + x * 4);

    *pixel = col.PackBGRA();
}

void ShapeRenderer::Clear(Color col)
{
    // for (UINT y = 0; y < m_Height; ++y)
    // {
    //     uint32_t* row = reinterpret_cast<uint32_t*>(m_Data + y * m_Stride);
    // 
    //     std::fill(row, row + m_Width, col.PackBGRA());
    // }

    for (UINT y = 0; y < m_Height; ++y)
    {
        for (UINT x = 0; x < m_Width; ++x)
        {
            SetPixel(x, y, col);
        }
    }
}
