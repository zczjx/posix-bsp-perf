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
#include <iostream>
#include <shared/ArgParser.hpp>
using namespace bsp_perf::perf_cases;
using namespace bsp_perf::shared;

int main(int argc, char* argv[])
{
    try {
        // 创建参数解析器
        bsp_perf::shared::ArgParser parser("EglRender");
        // 添加选项
        parser.addOption("--width", uint32_t(800), "Window width (0 for fullscreen)");
        parser.addOption("--height", uint32_t(600), "Window height (0 for fullscreen)");
        parser.addOption("--frames", uint32_t(300), "Maximum frames to render (0 for infinite)");
        parser.addOption("--fps", float(30.0f), "Target frame rate");
        parser.addFlag("--fullscreen", false, "Enable fullscreen mode");
        parser.addOption("--cycles", int32_t(1), "Running cycles for the perf case");

        // 设置配置文件支持
        parser.setConfig("--cfg", "egl_render.ini", "Configuration file path");

        // 解析参数
        parser.parseArgs(argc, argv);

        // 获取运行周期数
        int32_t cycles;
        parser.getOptionVal("--cycles", cycles);

        // 创建并运行应用
        EglRenderApp app(std::move(parser));
        app.run(cycles);

        std::cout << "\n✅ EGL Render Demo completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return -1;
    }
}

