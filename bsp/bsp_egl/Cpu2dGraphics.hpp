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

#ifndef CPU_2D_GRAPHICS_HPP
#define CPU_2D_GRAPHICS_HPP

#include <cstdint>
#include <vector>

namespace bsp_egl {

/**
 * @brief CPU 2D图形绘制类
 *
 * 提供基于CPU的2D图形绘制功能，直接操作framebuffer
 * 职责：纯粹的2D绘图操作（矩形、像素、图像等）
 */
class Cpu2dGraphics {
public:
    /**
     * @brief 颜色结构（RGBA）
     */
    struct Color {
        uint8_t r{0};
        uint8_t g{0};
        uint8_t b{0};
        uint8_t a{255};

        Color() = default;
        Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
            : r(red), g(green), b(blue), a(alpha) {}

        // 从浮点数创建 [0.0, 1.0] -> [0, 255]
        static Color fromFloat(float red, float green, float blue, float alpha = 1.0f) {
            return Color(
                static_cast<uint8_t>(red * 255),
                static_cast<uint8_t>(green * 255),
                static_cast<uint8_t>(blue * 255),
                static_cast<uint8_t>(alpha * 255)
            );
        }

        // 从HSV创建颜色 (H: 0-360, S: 0-1, V: 0-1)
        static Color fromHSV(float h, float s, float v);
    };

    /**
     * @brief 彩虹方向枚举
     */
    enum class RainbowDirection {
        Horizontal,  ///< 水平彩虹（从左到右）
        Vertical,    ///< 垂直彩虹（从上到下）
        Diagonal,    ///< 对角彩虹（从左上到右下）
        Radial       ///< 径向彩虹（从中心向外）
    };
    /**
     * @brief 构造函数
     * @param framebuffer framebuffer引用
     * @param width framebuffer宽度
     * @param height framebuffer高度
     */
    Cpu2dGraphics(std::vector<uint32_t>& framebuffer, uint32_t width, uint32_t height);

    /**
     * @brief 析构函数
     */
    ~Cpu2dGraphics() = default;

    // 禁用拷贝和移动
    Cpu2dGraphics(const Cpu2dGraphics&) = delete;
    Cpu2dGraphics& operator=(const Cpu2dGraphics&) = delete;
    Cpu2dGraphics(Cpu2dGraphics&&) = delete;
    Cpu2dGraphics& operator=(Cpu2dGraphics&&) = delete;

    /**
     * @brief 清屏（填充单色）
     * @param color 颜色
     */
    void clear(const Color& color);

    /**
     * @brief 绘制填充矩形
     * @param x 左上角X坐标（像素）
     * @param y 左上角Y坐标（像素）
     * @param width 宽度（像素）
     * @param height 高度（像素）
     * @param color 填充颜色
     */
    void fillRect(int x, int y, int width, int height, const Color& color);

    /**
     * @brief 绘制渐变矩形（水平渐变）
     * @param x 左上角X坐标（像素）
     * @param y 左上角Y坐标（像素）
     * @param width 宽度（像素）
     * @param height 高度（像素）
     * @param colorLeft 左边颜色
     * @param colorRight 右边颜色
     */
    void fillRectGradient(int x, int y, int width, int height,
                          const Color& colorLeft, const Color& colorRight);

    /**
     * @brief 设置单个像素
     * @param x X坐标（像素）
     * @param y Y坐标（像素）
     * @param color 颜色
     */
    void setPixel(int x, int y, const Color& color);

    /**
     * @brief 绘制七色彩虹背景
     * @param direction 彩虹方向（默认水平）
     * @param saturation 饱和度 0.0-1.0（默认1.0，越小越灰）
     * @param brightness 亮度 0.0-1.0（默认1.0，越小越暗）
     * @note 使用HSV色彩空间生成平滑的彩虹渐变
     */
    void fillRainbow(RainbowDirection direction = RainbowDirection::Horizontal,
                     float saturation = 1.0f,
                     float brightness = 1.0f);

    /**
     * @brief 获取framebuffer宽度
     */
    uint32_t getWidth() const { return m_width; }

    /**
     * @brief 获取framebuffer高度
     */
    uint32_t getHeight() const { return m_height; }

private:
    /**
     * @brief 混合颜色（考虑alpha）
     */
    uint32_t blendColor(uint32_t dst, const Color& src);

    /**
     * @brief 颜色转uint32_t (ARGB格式)
     */
    inline uint32_t colorToUint32(const Color& color) {
        return (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    }

private:
    std::vector<uint32_t>& m_framebuffer;  ///< framebuffer引用
    uint32_t m_width;                       ///< framebuffer宽度
    uint32_t m_height;                      ///< framebuffer高度
};

} // namespace bsp_egl

#endif // CPU_2D_GRAPHICS_HPP

