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
    config.title = "EGL Render Demo - GPU Accelerated (OpenGL ES 2.0)";
    config.fullscreen = m_fullscreen;

    // 初始化EGL
    if (m_render_egl->init(config) != 0)
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Error, "Failed to initialize RenderEGL");
        throw std::runtime_error("RenderEGL initialization failed");
    }

    // 创建Cpu2dGraphics实例（使用RenderEGL的framebuffer）
    // 注意：虽然使用CPU绘图，但最终通过OpenGL ES上传纹理到GPU渲染，比纯CPU快很多
    m_graphics = std::make_unique<bsp_egl::Cpu2dGraphics>(
        m_render_egl->getFramebuffer(),
        m_render_egl->getWidth(),
        m_render_egl->getHeight()
    );

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, 
        "EglRenderApp::onInit() completed successfully (GPU accelerated)");
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

    // 先释放graphics，再释放RenderEGL
    m_graphics.reset();
    m_render_egl.reset();

    m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "EglRenderApp::onRelease() completed");
}

void EglRenderApp::renderFrame(float deltaTime)
{
    m_time += deltaTime;
    m_frame_count++;
    m_rainbow_switch_time += deltaTime;

    // 每3秒切换一次彩虹方向
    if (m_rainbow_switch_time >= m_rainbow_duration) {
        m_rainbow_switch_time = 0.0f;
        m_rainbow_mode = (m_rainbow_mode + 1) % 4;

        const char* modeNames[] = {
            "Horizontal Rainbow (水平彩虹)",
            "Vertical Rainbow (垂直彩虹)",
            "Diagonal Rainbow (对角彩虹)",
            "Radial Rainbow (径向彩虹)"
        };
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "Switching to: {}", modeNames[m_rainbow_mode]);
    }

    // 1. 绘制动态彩虹背景（带呼吸效果）
    bsp_egl::Cpu2dGraphics::RainbowDirection directions[] = {
        bsp_egl::Cpu2dGraphics::RainbowDirection::Horizontal,
        bsp_egl::Cpu2dGraphics::RainbowDirection::Vertical,
        bsp_egl::Cpu2dGraphics::RainbowDirection::Diagonal,
        bsp_egl::Cpu2dGraphics::RainbowDirection::Radial
    };

    // 使用正弦波创造呼吸效果
    float breathe = std::sin(m_time * 1.5f) * 0.15f + 0.85f;  // 0.7 - 1.0
    m_graphics->fillRainbow(directions[m_rainbow_mode], 1.0f, breathe);

    // 2. 绘制半透明渐变叠加层（增加深度感）
    auto color_overlay_left = bsp_egl::Cpu2dGraphics::Color(0, 0, 0, 30);
    auto color_overlay_right = bsp_egl::Cpu2dGraphics::Color(0, 0, 0, 80);
    m_graphics->fillRectGradient(0, 0,
                                   m_graphics->getWidth(),
                                   m_graphics->getHeight(),
                                   color_overlay_left, color_overlay_right);

    // 3. 绘制中央信息面板
    int panel_width = 500;
    int panel_height = 200;
    int panel_x = (m_graphics->getWidth() - panel_width) / 2;
    int panel_y = (m_graphics->getHeight() - panel_height) / 2;

    // 面板背景（半透明深色）
    m_graphics->fillRect(panel_x, panel_y, panel_width, panel_height,
                          bsp_egl::Cpu2dGraphics::Color(20, 20, 40, 200));

    // 面板边框（亮色）
    m_graphics->fillRect(panel_x - 5, panel_y - 5, panel_width + 10, 5,
                          bsp_egl::Cpu2dGraphics::Color(200, 200, 255, 220));
    m_graphics->fillRect(panel_x - 5, panel_y + panel_height, panel_width + 10, 5,
                          bsp_egl::Cpu2dGraphics::Color(200, 200, 255, 220));

    // 4. 绘制动画元素（旋转的彩色方块）
    float angle = m_time * 2.0f;
    for (int i = 0; i < 4; i++) {
        float r = 150.0f;
        float a = angle + i * 3.14159f / 2.0f;
        int x = m_graphics->getWidth() / 2 + static_cast<int>(std::cos(a) * r);
        int y = m_graphics->getHeight() / 2 + static_cast<int>(std::sin(a) * r);

        // 彩色方块（颜色随时间变化）
        float hue = m_time * 50.0f + i * 90.0f;
        auto color = bsp_egl::Cpu2dGraphics::Color::fromHSV(hue, 1.0f, 1.0f);
        m_graphics->fillRect(x - 25, y - 25, 50, 50, color);
    }

    // 5. 绘制进度条（显示当前模式的进度）
    int progress_width = static_cast<int>((m_rainbow_switch_time / m_rainbow_duration) *
                                          (m_graphics->getWidth() - 40));
    m_graphics->fillRect(20, m_graphics->getHeight() - 30,
                          m_graphics->getWidth() - 40, 20,
                          bsp_egl::Cpu2dGraphics::Color(50, 50, 50, 180));
    m_graphics->fillRect(20, m_graphics->getHeight() - 30,
                          progress_width, 20,
                          bsp_egl::Cpu2dGraphics::Color(100, 255, 100, 220));

    // 6. 绘制装饰性的小方块（模拟文字/图标）
    // 左上角 - 帧数指示器
    m_graphics->fillRect(10, 10, 150, 40,
                          bsp_egl::Cpu2dGraphics::Color(0, 0, 0, 180));
    for (int i = 0; i < 5; i++) {
        m_graphics->fillRect(20 + i * 25, 20, 20, 20,
                              bsp_egl::Cpu2dGraphics::Color(255, 200, 0, 255));
    }

    // 右上角 - 模式指示器
    const char* modeNames[] = {"H", "V", "D", "R"};
    for (int i = 0; i < 4; i++) {
        auto box_color = (i == m_rainbow_mode) ?
            bsp_egl::Cpu2dGraphics::Color(255, 255, 255, 255) :
            bsp_egl::Cpu2dGraphics::Color(100, 100, 100, 180);
        m_graphics->fillRect(m_graphics->getWidth() - 200 + i * 45, 10, 40, 40, box_color);
    }

    // 每60帧输出一次日志
    if (m_frame_count % 60 == 0) {
        const char* modeName[] = {
            "Horizontal", "Vertical", "Diagonal", "Radial"
        };
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info,
            "Frame: {} | Mode: {} | Progress: {:.1f}%",
            m_frame_count,
            modeName[m_rainbow_mode],
            (m_rainbow_switch_time / m_rainbow_duration) * 100.0f);
    }
}
