/*
MIT License

Copyright (c) 2024 Clarence Zhou<287334895@qq.com> and contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Cpu2dGraphics.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace bsp_egl {

// 静态辅助函数：HSV到RGB转换
Cpu2dGraphics::Color Cpu2dGraphics::Color::fromHSV(float h, float s, float v) {
    // 确保参数在有效范围内
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    
    float c = v * s;  // Chroma
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r, g, b;
    
    if (h < 60.0f) {
        r = c; g = x; b = 0;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0;
    } else if (h < 180.0f) {
        r = 0; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    return Color(
        static_cast<uint8_t>((r + m) * 255),
        static_cast<uint8_t>((g + m) * 255),
        static_cast<uint8_t>((b + m) * 255),
        255
    );
}

Cpu2dGraphics::Cpu2dGraphics(std::vector<uint32_t>& framebuffer, uint32_t width, uint32_t height)
    : m_framebuffer(framebuffer)
    , m_width(width)
    , m_height(height)
{
}

void Cpu2dGraphics::clear(const Color& color)
{
    uint32_t pixel = colorToUint32(color);
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), pixel);
}

void Cpu2dGraphics::fillRect(int x, int y, int width, int height, const Color& color)
{
    // 边界检查
    if (x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return;
    if (x + width <= 0 || y + height <= 0) return;

    // 裁剪到窗口范围
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(static_cast<int>(m_width), x + width);
    int y2 = std::min(static_cast<int>(m_height), y + height);

    uint32_t pixel = colorToUint32(color);

    // 填充矩形
    for (int row = y1; row < y2; ++row) {
        uint32_t* line = &m_framebuffer[row * m_width];
        for (int col = x1; col < x2; ++col) {
            if (color.a == 255) {
                line[col] = pixel;
            } else {
                // Alpha混合
                line[col] = blendColor(line[col], color);
            }
        }
    }
}

void Cpu2dGraphics::fillRectGradient(int x, int y, int width, int height,
                                      const Color& colorLeft, const Color& colorRight)
{
    // 边界检查
    if (x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return;
    if (x + width <= 0 || y + height <= 0) return;

    // 裁剪到窗口范围
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(static_cast<int>(m_width), x + width);
    int y2 = std::min(static_cast<int>(m_height), y + height);

    // 水平渐变
    for (int row = y1; row < y2; ++row) {
        uint32_t* line = &m_framebuffer[row * m_width];
        for (int col = x1; col < x2; ++col) {
            // 计算插值比例
            float t = static_cast<float>(col - x) / width;
            t = std::clamp(t, 0.0f, 1.0f);

            // 插值颜色
            Color c;
            c.r = colorLeft.r + static_cast<uint8_t>((colorRight.r - colorLeft.r) * t);
            c.g = colorLeft.g + static_cast<uint8_t>((colorRight.g - colorLeft.g) * t);
            c.b = colorLeft.b + static_cast<uint8_t>((colorRight.b - colorLeft.b) * t);
            c.a = colorLeft.a + static_cast<uint8_t>((colorRight.a - colorLeft.a) * t);

            if (c.a == 255) {
                line[col] = colorToUint32(c);
            } else {
                line[col] = blendColor(line[col], c);
            }
        }
    }
}

void Cpu2dGraphics::setPixel(int x, int y, const Color& color)
{
    if (x < 0 || x >= static_cast<int>(m_width) ||
        y < 0 || y >= static_cast<int>(m_height)) {
        return;
    }

    uint32_t& pixel = m_framebuffer[y * m_width + x];
    if (color.a == 255) {
        pixel = colorToUint32(color);
    } else {
        pixel = blendColor(pixel, color);
    }
}

void Cpu2dGraphics::fillRainbow(RainbowDirection direction,
                                 float saturation,
                                 float brightness)
{
    // 限制参数范围
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    brightness = std::clamp(brightness, 0.0f, 1.0f);

    switch (direction) {
        case RainbowDirection::Horizontal: {
            // 水平彩虹：从左（红色，H=0）到右（紫色，H=300）
            for (uint32_t y = 0; y < m_height; ++y) {
                uint32_t* line = &m_framebuffer[y * m_width];
                for (uint32_t x = 0; x < m_width; ++x) {
                    float t = static_cast<float>(x) / (m_width - 1);
                    float hue = t * 300.0f;  // 0-300度，完整彩虹色谱
                    Color c = Color::fromHSV(hue, saturation, brightness);
                    line[x] = colorToUint32(c);
                }
            }
            break;
        }

        case RainbowDirection::Vertical: {
            // 垂直彩虹：从上（红色）到下（紫色）
            for (uint32_t y = 0; y < m_height; ++y) {
                float t = static_cast<float>(y) / (m_height - 1);
                float hue = t * 300.0f;
                Color c = Color::fromHSV(hue, saturation, brightness);
                uint32_t pixel = colorToUint32(c);

                uint32_t* line = &m_framebuffer[y * m_width];
                for (uint32_t x = 0; x < m_width; ++x) {
                    line[x] = pixel;
                }
            }
            break;
        }

        case RainbowDirection::Diagonal: {
            // 对角彩虹：从左上到右下
            float maxDist = std::sqrt(m_width * m_width + m_height * m_height);
            for (uint32_t y = 0; y < m_height; ++y) {
                uint32_t* line = &m_framebuffer[y * m_width];
                for (uint32_t x = 0; x < m_width; ++x) {
                    float dist = std::sqrt(x * x + y * y);
                    float t = dist / maxDist;
                    float hue = t * 300.0f;
                    Color c = Color::fromHSV(hue, saturation, brightness);
                    line[x] = colorToUint32(c);
                }
            }
            break;
        }

        case RainbowDirection::Radial: {
            // 径向彩虹：从中心向外
            float centerX = m_width / 2.0f;
            float centerY = m_height / 2.0f;
            float maxRadius = std::sqrt(centerX * centerX + centerY * centerY);

            for (uint32_t y = 0; y < m_height; ++y) {
                uint32_t* line = &m_framebuffer[y * m_width];
                for (uint32_t x = 0; x < m_width; ++x) {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float radius = std::sqrt(dx * dx + dy * dy);
                    float t = radius / maxRadius;
                    float hue = t * 300.0f;
                    Color c = Color::fromHSV(hue, saturation, brightness);
                    line[x] = colorToUint32(c);
                }
            }
            break;
        }
    }
}

uint32_t Cpu2dGraphics::blendColor(uint32_t dst, const Color& src)
{
    if (src.a == 0) return dst;
    if (src.a == 255) return colorToUint32(src);

    // 提取目标颜色分量
    uint8_t dst_a = (dst >> 24) & 0xFF;
    uint8_t dst_r = (dst >> 16) & 0xFF;
    uint8_t dst_g = (dst >> 8) & 0xFF;
    uint8_t dst_b = dst & 0xFF;

    // Alpha混合
    float alpha = src.a / 255.0f;
    uint8_t out_r = src.r * alpha + dst_r * (1.0f - alpha);
    uint8_t out_g = src.g * alpha + dst_g * (1.0f - alpha);
    uint8_t out_b = src.b * alpha + dst_b * (1.0f - alpha);
    uint8_t out_a = std::min(255, src.a + dst_a);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

} // namespace bsp_egl

