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
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

namespace bsp_egl {

// 顶点着色器源码
static const char* VERTEX_SHADER_SOURCE = R"(
attribute vec2 a_position;
attribute vec2 a_texcoord;
varying vec2 v_texcoord;

void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
}
)";

// 片段着色器源码
static const char* FRAGMENT_SHADER_SOURCE = R"(
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D u_texture;

void main() {
    gl_FragColor = texture2D(u_texture, v_texcoord);
}
)";

RenderEGL::RenderEGL()
    : m_x_display(nullptr)
    , m_x_window(0)
    , m_wm_delete_window(0)
    , m_egl_display(EGL_NO_DISPLAY)
    , m_egl_surface(EGL_NO_SURFACE)
    , m_egl_context(EGL_NO_CONTEXT)
    , m_egl_config(nullptr)
    , m_texture(0)
    , m_shader_program(0)
    , m_vbo(0)
    , m_position_attr(0)
    , m_texcoord_attr(0)
    , m_texture_uniform(0)
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

    // 3. 初始化OpenGL ES
    if (initOpenGLES() != 0) {
        std::cerr << "Failed to initialize OpenGL ES" << std::endl;
        cleanup();
        return -1;
    }

    // 4. 初始化帧缓冲
    if (initFramebuffer() != 0) {
        std::cerr << "Failed to initialize framebuffer" << std::endl;
        cleanup();
        return -1;
    }

    m_initialized = true;
    std::cout << "✅ RenderEGL initialized successfully (GPU accelerated)" << std::endl;
    std::cout << "   Resolution: " << m_width << "x" << m_height << std::endl;
    std::cout << "   Backend: EGL + OpenGL ES 2.0" << std::endl;

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

    // 显示窗口
    XMapWindow(m_x_display, m_x_window);
    XFlush(m_x_display);

    std::cout << "✅ X11 window created: " << m_width << "x" << m_height << std::endl;
    return 0;
}

int RenderEGL::initEGL()
{
    // 1. 获取EGL Display
    m_egl_display = eglGetDisplay((EGLNativeDisplayType)m_x_display);
    if (m_egl_display == EGL_NO_DISPLAY) {
        std::cerr << "Cannot get EGL display" << std::endl;
        return -1;
    }

    // 2. 初始化EGL
    EGLint major, minor;
    if (!eglInitialize(m_egl_display, &major, &minor)) {
        std::cerr << "Cannot initialize EGL" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL initialized: version " << major << "." << minor << std::endl;

    // 3. 选择EGL配置
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    EGLint num_configs;
    if (!eglChooseConfig(m_egl_display, config_attribs, &m_egl_config, 1, &num_configs) || 
        num_configs == 0) {
        std::cerr << "Cannot choose EGL config" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL config selected" << std::endl;

    // 4. 创建EGL Surface
    m_egl_surface = eglCreateWindowSurface(m_egl_display, m_egl_config, 
                                          (EGLNativeWindowType)m_x_window, nullptr);
    if (m_egl_surface == EGL_NO_SURFACE) {
        std::cerr << "Cannot create EGL surface" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL surface created" << std::endl;

    // 5. 创建EGL Context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,  // OpenGL ES 2.0
        EGL_NONE
    };
    m_egl_context = eglCreateContext(m_egl_display, m_egl_config, 
                                     EGL_NO_CONTEXT, context_attribs);
    if (m_egl_context == EGL_NO_CONTEXT) {
        std::cerr << "Cannot create EGL context" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL context created (OpenGL ES 2.0)" << std::endl;

    // 6. 激活Context
    if (!eglMakeCurrent(m_egl_display, m_egl_surface, m_egl_surface, m_egl_context)) {
        std::cerr << "Cannot make EGL context current" << std::endl;
        return -1;
    }
    std::cout << "✅ EGL context activated" << std::endl;

    return 0;
}

GLuint RenderEGL::compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // 检查编译状态
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 0) {
            char* log = new char[log_length];
            glGetShaderInfoLog(shader, log_length, nullptr, log);
            std::cerr << "Shader compile error: " << log << std::endl;
            delete[] log;
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

int RenderEGL::createShaderProgram()
{
    // 编译顶点着色器
    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE);
    if (vertex_shader == 0) {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return -1;
    }

    // 编译片段着色器
    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE);
    if (fragment_shader == 0) {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        glDeleteShader(vertex_shader);
        return -1;
    }

    // 创建程序并链接
    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, vertex_shader);
    glAttachShader(m_shader_program, fragment_shader);
    glLinkProgram(m_shader_program);

    // 检查链接状态
    GLint linked = 0;
    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint log_length = 0;
        glGetProgramiv(m_shader_program, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 0) {
            char* log = new char[log_length];
            glGetProgramInfoLog(m_shader_program, log_length, nullptr, log);
            std::cerr << "Program link error: " << log << std::endl;
            delete[] log;
        }
        glDeleteProgram(m_shader_program);
        m_shader_program = 0;
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return -1;
    }

    // 着色器已经链接到程序，可以删除
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // 获取属性和uniform位置
    m_position_attr = glGetAttribLocation(m_shader_program, "a_position");
    m_texcoord_attr = glGetAttribLocation(m_shader_program, "a_texcoord");
    m_texture_uniform = glGetUniformLocation(m_shader_program, "u_texture");

    return 0;
}

int RenderEGL::initOpenGLES()
{
    // 设置视口
    glViewport(0, 0, m_width, m_height);

    // 创建着色器程序
    if (createShaderProgram() != 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return -1;
    }
    std::cout << "✅ Shader program created" << std::endl;

    // 创建纹理
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::cout << "✅ Texture created" << std::endl;

    // 创建全屏四边形顶点数据
    // 顶点格式：x, y, u, v
    float vertices[] = {
        // 位置          // 纹理坐标
        -1.0f, -1.0f,   0.0f, 1.0f,  // 左下
         1.0f, -1.0f,   1.0f, 1.0f,  // 右下
        -1.0f,  1.0f,   0.0f, 0.0f,  // 左上
         1.0f,  1.0f,   1.0f, 0.0f,  // 右上
    };

    // 创建VBO
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    std::cout << "✅ VBO created" << std::endl;

    // 禁用深度测试（2D渲染不需要）
    glDisable(GL_DEPTH_TEST);

    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "✅ OpenGL ES initialized" << std::endl;
    return 0;
}

int RenderEGL::initFramebuffer()
{
    // 分配帧缓冲内存（ARGB格式，每像素4字节）
    m_framebuffer.resize(m_width * m_height, 0xFF000000);  // 默认黑色

    std::cout << "✅ Framebuffer initialized: " << m_width * m_height * 4 
              << " bytes" << std::endl;
    return 0;
}

void RenderEGL::cleanup()
{
    if (!m_initialized && m_egl_display == EGL_NO_DISPLAY) {
        return;
    }

    // 清理OpenGL ES资源
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_shader_program) {
        glDeleteProgram(m_shader_program);
        m_shader_program = 0;
    }

    // 清理帧缓冲
    m_framebuffer.clear();

    // 清理EGL
    if (m_egl_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(m_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (m_egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_egl_display, m_egl_context);
            m_egl_context = EGL_NO_CONTEXT;
        }
        if (m_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_egl_display, m_egl_surface);
            m_egl_surface = EGL_NO_SURFACE;
        }
        eglTerminate(m_egl_display);
        m_egl_display = EGL_NO_DISPLAY;
    }

    // 清理X11
    if (m_x_display) {
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

void RenderEGL::clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderEGL::renderFramebuffer()
{
    // 上传framebuffer数据到纹理
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_framebuffer.data());

    // 使用着色器程序
    glUseProgram(m_shader_program);

    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(m_texture_uniform, 0);

    // 设置顶点属性
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(m_position_attr);
    glVertexAttribPointer(m_position_attr, 2, GL_FLOAT, GL_FALSE, 
                         4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(m_texcoord_attr);
    glVertexAttribPointer(m_texcoord_attr, 2, GL_FLOAT, GL_FALSE,
                         4 * sizeof(float), (void*)(2 * sizeof(float)));

    // 绘制全屏四边形
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 清理
    glDisableVertexAttribArray(m_position_attr);
    glDisableVertexAttribArray(m_texcoord_attr);
}

void RenderEGL::swapBuffers()
{
    // 渲染framebuffer到屏幕
    renderFramebuffer();

    // 交换EGL缓冲区
    eglSwapBuffers(m_egl_display, m_egl_surface);
}

void RenderEGL::blitBuffer(const uint32_t* srcBuffer,
                           uint32_t srcWidth,
                           uint32_t srcHeight,
                           int dstX,
                           int dstY,
                           uint32_t srcX,
                           uint32_t srcY,
                           uint32_t copyWidth,
                           uint32_t copyHeight)
{
    if (!srcBuffer) {
        std::cerr << "blitBuffer: srcBuffer is null" << std::endl;
        return;
    }

    if (!m_initialized) {
        std::cerr << "blitBuffer: RenderEGL not initialized" << std::endl;
        return;
    }

    // 如果copyWidth/copyHeight为0，使用完整尺寸
    if (copyWidth == 0) copyWidth = srcWidth;
    if (copyHeight == 0) copyHeight = srcHeight;

    // 检查源矩形是否在源buffer范围内
    if (srcX >= srcWidth || srcY >= srcHeight) {
        return;
    }

    // 裁剪源矩形到源buffer边界
    if (srcX + copyWidth > srcWidth) {
        copyWidth = srcWidth - srcX;
    }
    if (srcY + copyHeight > srcHeight) {
        copyHeight = srcHeight - srcY;
    }

    // 裁剪目标矩形到framebuffer边界
    int copyStartX = std::max(0, dstX);
    int copyStartY = std::max(0, dstY);
    int copyEndX = std::min(static_cast<int>(m_width), dstX + static_cast<int>(copyWidth));
    int copyEndY = std::min(static_cast<int>(m_height), dstY + static_cast<int>(copyHeight));

    if (copyStartX >= copyEndX || copyStartY >= copyEndY) {
        return;  // 完全在framebuffer外
    }

    // 计算实际复制的尺寸
    int actualCopyWidth = copyEndX - copyStartX;
    int actualCopyHeight = copyEndY - copyStartY;

    // 计算源偏移
    uint32_t srcOffsetX = srcX + (copyStartX - dstX);
    uint32_t srcOffsetY = srcY + (copyStartY - dstY);

    // 快速内存复制（逐行复制）
    for (int y = 0; y < actualCopyHeight; ++y) {
        uint32_t* dstLine = &m_framebuffer[(copyStartY + y) * m_width + copyStartX];
        const uint32_t* srcLine = &srcBuffer[(srcOffsetY + y) * srcWidth + srcOffsetX];
        std::memcpy(dstLine, srcLine, actualCopyWidth * sizeof(uint32_t));
    }
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

    std::cout << "🎨 Starting render loop (GPU accelerated)" << std::endl;
    std::cout << "   Max frames: " << maxFrames << std::endl;
    std::cout << "   Target FPS: " << fps << std::endl;

    while (maxFrames == 0 || frame_count < maxFrames) {
        auto current_time = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        // 调用用户回调
        if (callback)
        {
            callback(delta_time);
        }

        // 交换缓冲区（GPU渲染）
        swapBuffers();

        frame_count++;

        // 帧率控制
        auto elapsed = std::chrono::steady_clock::now() - current_time;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }

        // 每30帧打印一次进度
        if (frame_count % 30 == 0) {
            std::cout << "📊 Rendered " << frame_count << " frames (GPU)" << std::endl;
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
