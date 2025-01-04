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


#ifndef HELLO_PERF_HPP
#define HELLO_PERF_HPP

#include <framework/BasePerfCase.hpp>
#include <shared/BspLogger.hpp>
#include <shared/ArgParser.hpp>
#include <memory>
#include <string>
#include <iostream>

namespace bsp_perf {
namespace perf_cases {
class HelloPerf : public bsp_perf::common::BasePerfCase
{

public:
    static constexpr char LOG_TAG[] {"[HelloPerf]: "};

    HelloPerf(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args)),
        m_logger{std::make_unique<bsp_perf::shared::BspLogger>("HelloPerf")}
    {
        m_logger->setPattern();
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::HelloPerf()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::HelloPerf()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::HelloPerf()", LOG_TAG);
    }
    HelloPerf(const HelloPerf&) = delete;
    HelloPerf& operator=(const HelloPerf&) = delete;
    HelloPerf(HelloPerf&&) = delete;
    HelloPerf& operator=(HelloPerf&&) = delete;
    ~HelloPerf()
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::~HelloPerf()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::~HelloPerf()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::~HelloPerf()", LOG_TAG);
    }
private:

    void onInit() override {
        auto& params = getArgs();
        std::string ret;
        params.getOptionVal("--append", ret);
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() append: {}", LOG_TAG, ret);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() name: {}", LOG_TAG, m_name);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() name: {}", LOG_TAG, m_name);

        std::vector<std::string> list;
        params.getOptionStrList("--list", list);

        for(auto& item : list)
        {
            m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() list item: {}", LOG_TAG, item);
            m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() list item: {}", LOG_TAG, item);
            m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onInit() list item: {}", LOG_TAG, item);
        }
    }

    void onProcess() override {
        bool ret;
        auto& params = getArgs();
        ret = params.getFlagVal("--enable");
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} HelloPerf::onProcess() ret: {}", LOG_TAG, ret);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} HelloPerf::onProcess()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Warn, "{} HelloPerf::onProcess()", LOG_TAG);
    }

    void onRender() override {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Critical, "{} HelloPerf::onRender()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Critical, "{} HelloPerf::onRender()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Critical, "{} HelloPerf::onRender()", LOG_TAG);
    }

    void onRelease() override {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onRelease()", LOG_TAG);
        m_logger->printFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onRelease()", LOG_TAG);
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Info, "{} HelloPerf::onRelease()", LOG_TAG);
    }

private:
    std::string m_name {"HelloPerf"};
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
};
} // namespace perf_cases
} // namespace bsp_perf


#endif // HELLO_PERF_HPP

