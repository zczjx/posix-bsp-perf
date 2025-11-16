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

#include "EglRenderApp.hpp"
#include <cmath>

using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

EglRenderApp::EglRenderApp(ArgParser&& args)
    : BasePerfCase(std::move(args)),
    m_logger{std::make_unique<bsp_perf::shared::BspLogger>("EglRenderApp")}
{
    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp constructor");
}

void EglRenderApp::onInit()
{
    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onInit() start");

    // 从配置读取参数
    auto& params = getArgs();
    params.getOptionVal("--width", m_window_width);
    params.getOptionVal("--height", m_window_height);
    params.getOptionVal("--frames", m_max_frames);
    params.getOptionVal("--fps", m_fps);
    m_fullscreen = params.getFlagVal("--fullscreen");

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
        "Configuration: width={} height={} max_frames={} fps={} fullscreen={}", 
        m_window_width, m_window_height, m_max_frames, m_fps, m_fullscreen);

    // 创建RenderEGL实例
    m_render_egl = std::make_unique<bsp_egl::RenderEGL>();

    // 配置窗口
    bsp_egl::RenderEGL::WindowConfig config;
    config.width = m_window_width;
    config.height = m_window_height;
    config.title = "EGL Render Demo - Pure CPU Drawing";
    config.fullscreen = m_fullscreen;

    // 初始化EGL
    if (m_render_egl->init(config) != 0) {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, "Failed to initialize RenderEGL");
        throw std::runtime_error("RenderEGL initialization failed");
    }

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onInit() completed successfully");
}

void EglRenderApp::onProcess()
{
    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onProcess() start");

    // 使用渲染循环
    m_render_egl->renderLoop(
        [this](float deltaTime) {
            this->renderFrame(deltaTime);
        },
        m_max_frames,
        m_fps
    );

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
        "EglRenderApp::onProcess() completed, rendered {} frames", m_frame_count);
}

void EglRenderApp::onRender()
{
    // 此方法在BasePerfCase::run()中被调用
    // 但我们使用renderLoop，所以这里留空
}

void EglRenderApp::onRelease()
{
    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onRelease() start");

    // RenderEGL会在析构函数中自动清理
    m_render_egl.reset();

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onRelease() completed");
}

void EglRenderApp::renderFrame(float deltaTime)
{
    m_time += deltaTime;
    m_frame_count++;

    // 1. 动态背景色（彩虹渐变）
    float hue = m_time * 0.5f;
    uint8_t r = static_cast<uint8_t>(std::abs(std::sin(hue)) * 50);
    uint8_t g = static_cast<uint8_t>(std::abs(std::sin(hue + 2.0f)) * 50);
    uint8_t b = static_cast<uint8_t>(std::abs(std::sin(hue + 4.0f)) * 50);
    
    m_render_egl->clear(bsp_egl::RenderEGL::Color(r, g, b));

    // 2. 绘制大矩形背景（渐变）
    auto color_left = bsp_egl::RenderEGL::Color(30, 30, 80, 180);
    auto color_right = bsp_egl::RenderEGL::Color(80, 30, 30, 180);
    m_render_egl->fillRectGradient(50, 50, 
                                    m_render_egl->getWidth() - 100,
                                    m_render_egl->getHeight() - 100,
                                    color_left, color_right);

    // 3. 绘制动画矩形（左右移动）
    int rect_width = 150;
    int rect_height = 150;
    int center_x = m_render_egl->getWidth() / 2;
    int center_y = m_render_egl->getHeight() / 2;
    int offset_x = static_cast<int>(std::sin(m_time * 2.0f) * 200);
    int offset_y = static_cast<int>(std::cos(m_time * 2.0f) * 100);
    
    m_render_egl->fillRect(
        center_x + offset_x - rect_width / 2,
        center_y + offset_y - rect_height / 2,
        rect_width,
        rect_height,
        bsp_egl::RenderEGL::Color(255, 0, 0, 200)
    );

    // 4. 绘制固定的彩色矩形（四个角）
    // 左上 - 绿色
    m_render_egl->fillRect(100, 100, 120, 120,
                            bsp_egl::RenderEGL::Color(0, 255, 0, 200));
    
    // 右上 - 蓝色
    m_render_egl->fillRect(m_render_egl->getWidth() - 220, 100, 120, 120,
                            bsp_egl::RenderEGL::Color(0, 0, 255, 200));
    
    // 左下 - 黄色
    m_render_egl->fillRect(100, m_render_egl->getHeight() - 220, 120, 120,
                            bsp_egl::RenderEGL::Color(255, 255, 0, 200));
    
    // 右下 - 紫色
    m_render_egl->fillRect(m_render_egl->getWidth() - 220, 
                            m_render_egl->getHeight() - 220, 120, 120,
                            bsp_egl::RenderEGL::Color(255, 0, 255, 200));

    // 5. 绘制一些小的渐变条
    for (int i = 0; i < 5; i++) {
        int y = 250 + i * 40;
        auto c1 = bsp_egl::RenderEGL::Color(255, i * 50, 0);
        auto c2 = bsp_egl::RenderEGL::Color(0, 255, i * 50);
        m_render_egl->fillRectGradient(200, y, 400, 30, c1, c2);
    }
}
