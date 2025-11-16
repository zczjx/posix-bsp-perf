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

#ifndef RENDER_EGL_HPP
#define RENDER_EGL_HPP

#include <EGL/egl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>

namespace bsp_egl {

/**
 * @brief 跨平台EGL渲染类，支持RK3588和Jetson Orin NX
 * 
 * 纯EGL封装，不依赖OpenGL ES，使用CPU直接绘制像素
 */
class RenderEGL {
public:
    /**
     * @brief 窗口配置参数
     */
    struct WindowConfig {
        uint32_t width{800};        ///< 窗口宽度，0表示全屏
        uint32_t height{600};       ///< 窗口高度，0表示全屏
        uint32_t x_offset{0};       ///< 水平偏移
        uint32_t y_offset{0};       ///< 垂直偏移
        std::string title{"RenderEGL Window"};  ///< 窗口标题
        bool fullscreen{false};     ///< 是否全屏
    };

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
    };

    /**
     * @brief 渲染回调函数类型
     * @param deltaTime 距离上一帧的时间差（秒）
     */
    using RenderCallback = std::function<void(float deltaTime)>;

    /**
     * @brief 构造函数
     */
    RenderEGL();

    /**
     * @brief 析构函数
     */
    ~RenderEGL();

    // 禁用拷贝和移动
    RenderEGL(const RenderEGL&) = delete;
    RenderEGL& operator=(const RenderEGL&) = delete;
    RenderEGL(RenderEGL&&) = delete;
    RenderEGL& operator=(RenderEGL&&) = delete;

    /**
     * @brief 初始化EGL和窗口
     * @param config 窗口配置参数
     * @return 成功返回0，失败返回-1
     */
    int init(const WindowConfig& config);

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 判断是否已初始化
     * @return true表示已初始化
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief 清屏（填充单色）
     * @param color 颜色
     */
    void clear(const Color& color);

    /**
     * @brief 交换前后缓冲区（显示渲染结果）
     */
    void swapBuffers();

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
     * @brief 获取窗口宽度
     */
    uint32_t getWidth() const { return m_width; }

    /**
     * @brief 获取窗口高度
     */
    uint32_t getHeight() const { return m_height; }

    /**
     * @brief 获取EGL Display
     */
    EGLDisplay getEGLDisplay() const { return m_egl_display; }

    /**
     * @brief 获取X11 Display
     */
    Display* getX11Display() const { return m_x_display; }

    /**
     * @brief 获取X11 Window
     */
    Window getX11Window() const { return m_x_window; }

    /**
     * @brief 处理X11事件（非阻塞）
     * @return 如果接收到关闭事件返回false，否则返回true
     */
    bool processEvents();

    /**
     * @brief 渲染循环（阻塞式）
     * @param callback 每帧调用的回调函数
     * @param maxFrames 最大帧数，0表示无限循环
     * @param fps 目标帧率
     */
    void renderLoop(RenderCallback callback, uint32_t maxFrames = 0, float fps = 30.0f);

    /**
     * @brief 获取显示器分辨率
     * @param width 输出宽度
     * @param height 输出高度
     * @return 成功返回0，失败返回-1
     */
    static int getDisplayResolution(uint32_t& width, uint32_t& height);

private:
    // X11相关
    Display* m_x_display;
    Window m_x_window;
    XImage* m_x_image;
    GC m_gc;
    Atom m_wm_delete_window;
    
    // EGL相关
    EGLDisplay m_egl_display;
    
    // 窗口参数
    uint32_t m_width;
    uint32_t m_height;
    
    // 帧缓冲（RGBA格式）
    std::vector<uint32_t> m_framebuffer;
    
    // 状态
    bool m_initialized;
    
    /**
     * @brief 初始化X11窗口
     */
    int initX11(const WindowConfig& config);
    
    /**
     * @brief 初始化EGL
     */
    int initEGL();
    
    /**
     * @brief 初始化帧缓冲
     */
    int initFramebuffer();
    
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
};

} // namespace bsp_egl

#endif // RENDER_EGL_HPP
