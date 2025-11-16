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

#include "RenderEGL.hpp"
#include <X11/keysym.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

namespace bsp_egl {

RenderEGL::RenderEGL()
    : m_x_display(nullptr)
    , m_x_window(0)
    , m_x_image(nullptr)
    , m_gc(0)
    , m_wm_delete_window(0)
    , m_egl_display(EGL_NO_DISPLAY)
    , m_width(0)
    , m_height(0)
    , m_initialized(false)
{
}

RenderEGL::~RenderEGL()
{
    cleanup();
}

int RenderEGL::init(const WindowConfig& config)
{
    if (m_initialized) {
        std::cerr << "RenderEGL already initialized" << std::endl;
        return -1;
    }

    // 1. 初始化X11窗口
    if (initX11(config) != 0) {
        std::cerr << "Failed to initialize X11" << std::endl;
        return -1;
    }

    // 2. 初始化EGL
    if (initEGL() != 0) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        cleanup();
        return -1;
    }

    // 3. 初始化帧缓冲
    if (initFramebuffer() != 0) {
        std::cerr << "Failed to initialize framebuffer" << std::endl;
        cleanup();
        return -1;
    }

    m_initialized = true;
    std::cout << "✅ RenderEGL initialized successfully (" 
              << m_width << "x" << m_height << ")" << std::endl;

    return 0;
}

int RenderEGL::initX11(const WindowConfig& config)
{
    // 打开X Display
    m_x_display = XOpenDisplay(nullptr);
    if (!m_x_display) {
        std::cerr << "Cannot open X display" << std::endl;
        return -1;
    }

    int screen_num = DefaultScreen(m_x_display);
    
    // 确定窗口尺寸
    if (config.fullscreen || (config.width == 0 || config.height == 0)) {
        m_width = DisplayWidth(m_x_display, screen_num);
        m_height = DisplayHeight(m_x_display, screen_num);
    } else {
        m_width = config.width;
        m_height = config.height;
    }

    // 创建窗口
    Window root = DefaultRootWindow(m_x_display);
    XSetWindowAttributes window_attrs;
    memset(&window_attrs, 0, sizeof(window_attrs));
    window_attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
    window_attrs.background_pixel = BlackPixel(m_x_display, screen_num);

    m_x_window = XCreateWindow(
        m_x_display, root,
        config.x_offset, config.y_offset,
        m_width, m_height,
        0, // border width
        CopyFromParent, // depth
        InputOutput, // class
        CopyFromParent, // visual
        CWBackPixel | CWEventMask,
        &window_attrs
    );

    if (!m_x_window) {
        std::cerr << "Cannot create X window" << std::endl;
        XCloseDisplay(m_x_display);
        m_x_display = nullptr;
        return -1;
    }

    // 设置窗口标题
    XStoreName(m_x_display, m_x_window, config.title.c_str());

    // 设置窗口关闭事件处理
    m_wm_delete_window = XInternAtom(m_x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(m_x_display, m_x_window, &m_wm_delete_window, 1);

    // 创建GC（图形上下文）
    m_gc = XCreateGC(m_x_display, m_x_window, 0, nullptr);

    // 显示窗口
    XMapWindow(m_x_display, m_x_window);
    XFlush(m_x_display);

    std::cout << "✅ X11 window created: " << m_width << "x" << m_height << std::endl;
    return 0;
}

int RenderEGL::initEGL()
{
    // 获取EGL Display
    m_egl_display = eglGetDisplay((EGLNativeDisplayType)m_x_display);
    if (m_egl_display == EGL_NO_DISPLAY) {
        std::cerr << "Cannot get EGL display" << std::endl;
        return -1;
    }

    // 初始化EGL
    EGLint major, minor;
    if (!eglInitialize(m_egl_display, &major, &minor)) {
        std::cerr << "Cannot initialize EGL" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL initialized: version " << major << "." << minor << std::endl;

    return 0;
}

int RenderEGL::initFramebuffer()
{
    // 分配帧缓冲内存（ARGB格式，每像素4字节）
    m_framebuffer.resize(m_width * m_height, 0xFF000000);  // 默认黑色

    // 创建XImage用于将帧缓冲绘制到窗口
    Visual* visual = DefaultVisual(m_x_display, DefaultScreen(m_x_display));
    int depth = DefaultDepth(m_x_display, DefaultScreen(m_x_display));

    m_x_image = XCreateImage(
        m_x_display,
        visual,
        depth,
        ZPixmap,
        0,
        reinterpret_cast<char*>(m_framebuffer.data()),
        m_width,
        m_height,
        32,  // bitmap_pad
        m_width * 4  // bytes_per_line
    );

    if (!m_x_image) {
        std::cerr << "Cannot create XImage" << std::endl;
        return -1;
    }

    std::cout << "✅ Framebuffer initialized: " << m_width * m_height * 4 
              << " bytes" << std::endl;
    return 0;
}

void RenderEGL::cleanup()
{
    if (!m_initialized && m_egl_display == EGL_NO_DISPLAY) {
        return;
    }

    // 清理XImage
    if (m_x_image) {
        m_x_image->data = nullptr;  // 防止XDestroyImage释放我们的framebuffer
        XDestroyImage(m_x_image);
        m_x_image = nullptr;
    }

    // 清理帧缓冲
    m_framebuffer.clear();

    // 清理EGL
    if (m_egl_display != EGL_NO_DISPLAY) {
        eglTerminate(m_egl_display);
        m_egl_display = EGL_NO_DISPLAY;
    }

    // 清理X11
    if (m_x_display) {
        if (m_gc) {
            XFreeGC(m_x_display, m_gc);
            m_gc = 0;
        }
        if (m_x_window) {
            XDestroyWindow(m_x_display, m_x_window);
            m_x_window = 0;
        }
        XCloseDisplay(m_x_display);
        m_x_display = nullptr;
    }

    m_initialized = false;
    std::cout << "✅ RenderEGL cleaned up" << std::endl;
}

void RenderEGL::clear(const Color& color)
{
    uint32_t pixel = colorToUint32(color);
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), pixel);
}

void RenderEGL::swapBuffers()
{
    // 将帧缓冲绘制到X11窗口
    XPutImage(
        m_x_display,
        m_x_window,
        m_gc,
        m_x_image,
        0, 0,      // src_x, src_y
        0, 0,      // dest_x, dest_y
        m_width,
        m_height
    );
    XFlush(m_x_display);
}

void RenderEGL::fillRect(int x, int y, int width, int height, const Color& color)
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

void RenderEGL::fillRectGradient(int x, int y, int width, int height,
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

void RenderEGL::setPixel(int x, int y, const Color& color)
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

uint32_t RenderEGL::blendColor(uint32_t dst, const Color& src)
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

bool RenderEGL::processEvents()
{
    while (XPending(m_x_display) > 0) {
        XEvent event;
        XNextEvent(m_x_display, &event);

        if (event.type == ClientMessage) {
            if (static_cast<Atom>(event.xclient.data.l[0]) == m_wm_delete_window) {
                return false;  // 窗口关闭
            }
        } else if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape || key == XK_q) {
                return false;  // ESC或Q键退出
            }
        }
    }
    return true;
}

void RenderEGL::renderLoop(RenderCallback callback, uint32_t maxFrames, float fps)
{
    if (!m_initialized) {
        std::cerr << "RenderEGL not initialized" << std::endl;
        return;
    }

    const auto frame_duration = std::chrono::duration<double>(1.0 / fps);
    auto last_time = std::chrono::steady_clock::now();
    uint32_t frame_count = 0;

    std::cout << "🎨 Starting render loop (max_frames=" << maxFrames 
              << ", fps=" << fps << ")" << std::endl;

    while (maxFrames == 0 || frame_count < maxFrames) {
        auto current_time = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        // 处理事件
        if (!processEvents()) {
            std::cout << "Received exit signal" << std::endl;
            break;
        }

        // 调用用户回调
        if (callback) {
            callback(delta_time);
        }

        // 交换缓冲区
        swapBuffers();

        frame_count++;

        // 帧率控制
        auto elapsed = std::chrono::steady_clock::now() - current_time;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }

        // 每30帧打印一次进度
        if (frame_count % 30 == 0) {
            std::cout << "📊 Rendered " << frame_count << " frames" << std::endl;
        }
    }

    std::cout << "✅ Render loop finished, total frames: " << frame_count << std::endl;
}

int RenderEGL::getDisplayResolution(uint32_t& width, uint32_t& height)
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        return -1;
    }

    int screen_num = DefaultScreen(display);
    width = DisplayWidth(display, screen_num);
    height = DisplayHeight(display, screen_num);

    XCloseDisplay(display);
    return 0;
}

} // namespace bsp_egl
