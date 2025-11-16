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
#include <string>
#include <functional>
#include <cstdint>
#include <vector>

namespace bsp_egl {

/**
 * @brief 跨平台EGL渲染类，支持RK3588和Jetson Orin NX
 *
 * 职责：EGL上下文管理、X11窗口管理、framebuffer管理、显示输出
 * 不包含2D绘图逻辑（请使用Cpu2dGraphics类）
 * current support platform
 *   RK3588: EGL/X11
 *   Jetson Orin NX: EGL/X11
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
     * @brief 交换前后缓冲区（显示渲染结果）
     */
    void swapBuffers();

    /**
     * @brief 获取framebuffer引用
     * @return framebuffer引用
     * @note 可以用于Cpu2dGraphics进行2D绘图操作
     */
    std::vector<uint32_t>& getFramebuffer() { return m_framebuffer; }

    /**
     * @brief 将外部ARGB32格式的像素buffer贴图到framebuffer
     * @param srcBuffer 源buffer指针（ARGB32格式）
     * @param srcWidth 源buffer宽度
     * @param srcHeight 源buffer高度
     * @param dstX 目标X坐标（在framebuffer中的位置，默认0）
     * @param dstY 目标Y坐标（在framebuffer中的位置，默认0）
     * @param srcX 源矩形左上角X坐标（默认0）
     * @param srcY 源矩形左上角Y坐标（默认0）
     * @param copyWidth 复制宽度（0表示使用完整宽度）
     * @param copyHeight 复制高度（0表示使用完整高度）
     * @note 此方法直接操作framebuffer，适用于需要高性能批量贴图的场景
     * @note 像素格式必须是ARGB32 (0xAARRGGBB)，与framebuffer格式一致
     */
    void blitBuffer(const uint32_t* srcBuffer,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    int dstX = 0,
                    int dstY = 0,
                    uint32_t srcX = 0,
                    uint32_t srcY = 0,
                    uint32_t copyWidth = 0,
                    uint32_t copyHeight = 0);

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
};

} // namespace bsp_egl

#endif // RENDER_EGL_HPP
