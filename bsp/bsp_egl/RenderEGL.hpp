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
#include <GLES2/gl2.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>

namespace bsp_egl {

/**
 * @brief 硬件加速EGL渲染类，支持RK3588和Jetson Orin NX
 *
 * 使用 EGL + OpenGL ES 2.0 进行GPU硬件加速渲染
 * 职责：
 *   - EGL上下文和Surface管理
 *   - X11窗口管理
 *   - OpenGL ES渲染管线
 *   - Framebuffer纹理上传和显示
 * 
 * 支持平台:
 *   RK3588: EGL + OpenGL ES (Mali GPU)
 *   Jetson Orin NX: EGL + OpenGL ES (NVIDIA GPU)
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
     * @note 使用OpenGL ES纹理上传和GPU渲染，性能优于CPU拷贝
     * @note 像素格式必须是ARGB32 (0xAARRGGBB)
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
     * @brief 清空屏幕为指定颜色（GPU加速）
     * @param r 红色分量 (0.0-1.0)
     * @param g 绿色分量 (0.0-1.0)
     * @param b 蓝色分量 (0.0-1.0)
     * @param a 透明度 (0.0-1.0)
     */
    void clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);

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
    Atom m_wm_delete_window;

    // EGL相关
    EGLDisplay m_egl_display;
    EGLSurface m_egl_surface;
    EGLContext m_egl_context;
    EGLConfig m_egl_config;

    // OpenGL ES 相关
    GLuint m_texture;           // 用于显示framebuffer的纹理
    GLuint m_shader_program;    // 着色器程序
    GLuint m_vbo;               // 顶点缓冲对象
    GLuint m_position_attr;     // 顶点位置属性
    GLuint m_texcoord_attr;     // 纹理坐标属性
    GLuint m_texture_uniform;   // 纹理uniform

    // 窗口参数
    uint32_t m_width;
    uint32_t m_height;

    // 帧缓冲（ARGB格式，用于CPU绘图）
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
     * @brief 初始化OpenGL ES（创建着色器、纹理等）
     */
    int initOpenGLES();

    /**
     * @brief 初始化帧缓冲
     */
    int initFramebuffer();

    /**
     * @brief 创建并编译着色器
     */
    GLuint compileShader(GLenum type, const char* source);

    /**
     * @brief 创建着色器程序
     */
    int createShaderProgram();

    /**
     * @brief 渲染framebuffer到屏幕（通过OpenGL ES）
     */
    void renderFramebuffer();
};

} // namespace bsp_egl

#endif // RENDER_EGL_HPP
