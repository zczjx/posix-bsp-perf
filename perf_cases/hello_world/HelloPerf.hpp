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
#include <iostream>

namespace bsp_perf {
namespace perf_cases {

using string_view_t = bsp_perf::shared::string_view_t;

class HelloPerf : public bsp_perf::common::BasePerfCase
{

public:
    static constexpr char LOG_TAG[] {"[HelloPerf]: "};

    HelloPerf()
    {
        // std::cout << LOG_TAG << "HelloPerf::HelloPerf()" << std::endl;
        m_logger = std::make_shared<bsp_perf::shared::BspLogger>();
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::HelloPerf()");
    }
    HelloPerf(const HelloPerf&) = delete;
    HelloPerf& operator=(const HelloPerf&) = delete;
    HelloPerf(HelloPerf&&) = delete;
    HelloPerf& operator=(HelloPerf&&) = delete;
    ~HelloPerf()
    {
        // std::cout << LOG_TAG << "HelloPerf::~HelloPerf()" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::~HelloPerf()");
    }
private:

    void onInit() override {
        // std::cout << LOG_TAG << "HelloPerf::onInit()" << std::endl;
        // std::cout << LOG_TAG << "name: " << m_name << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::onInit()");
    }

    void onProcess() override {
        // std::cout << LOG_TAG << "HelloPerf::onProcess()" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::onProcess()");
    }

    void onRender() override {
        // std::cout << LOG_TAG << "HelloPerf::onRender()" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::onRender()");
    }

    void onPerfPrint() override {
        // std::cout << LOG_TAG << "HelloPerf::onPerfPrint()" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::onPerfPrint()");
    }

    void onRelease() override {
        // std::cout << LOG_TAG << "HelloPerf::onRelease()" << std::endl;
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Debug, "{} HelloPerf::onRelease()");
    }

private:
    std::string m_name {"HelloPerf"};
    std::shared_ptr<bsp_perf::shared::BspLogger> m_logger;
};
} // namespace perf_cases
} // namespace bsp_perf


#endif // HELLO_PERF_HPP

