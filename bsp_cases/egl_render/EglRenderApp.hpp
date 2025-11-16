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

#ifndef EGL_RENDER_APP_HPP
#define EGL_RENDER_APP_HPP

#include <framework/BasePerfCase.hpp>
#include <bsp_egl/RenderEGL.hpp>
#include <bsp_egl/Cpu2dGraphics.hpp>
#include <shared/BspLogger.hpp>
#include <memory>

namespace bsp_perf {
namespace perf_cases {

/**
 * @brief EGL渲染示例应用
 * 
 * 演示如何使用RenderEGL和Cpu2dGraphics进行2D图形渲染
 * - RenderEGL: 负责窗口管理和显示输出
 * - Cpu2dGraphics: 负责2D绘图操作
 */
class EglRenderApp : public common::BasePerfCase {
public:
    EglRenderApp(shared::ArgParser&& args);
    virtual ~EglRenderApp() = default;

private:
    void onInit() override;
    void onProcess() override;
    void onRender() override;
    void onRelease() override;

    // 渲染回调函数
    void renderFrame(float deltaTime);

private:
    std::unique_ptr<bsp_egl::RenderEGL> m_render_egl;
    std::unique_ptr<bsp_egl::Cpu2dGraphics> m_graphics;

    // 动画参数
    float m_time{0.0f};
    uint32_t m_frame_count{0};

    // 彩虹参数
    int m_rainbow_mode{0};           // 当前彩虹模式
    float m_rainbow_switch_time{0.0f};  // 彩虹切换计时器
    const float m_rainbow_duration{3.0f};  // 每种彩虹显示时长（秒）

    // 配置参数
    uint32_t m_window_width{800};
    uint32_t m_window_height{600};
    uint32_t m_max_frames{300};  // 默认300帧（10秒@30fps）
    float m_fps{30.0f};
    bool m_fullscreen{false};

    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger{nullptr};
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // EGL_RENDER_APP_HPP
