
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
#ifndef BSP_LOGGER_HPP
#define BSP_LOGGER_HPP

// Your code here

#include <string>
#include <iostream>
#include <string>
#include <memory>
#include <string_view>
#include <spdlog/spdlog.h>

namespace bsp_perf {
namespace shared {

using namespace std::string_literals;

class BspLogger {
public:
    BspLogger(const std::string& log_file_path = "logs/bsp_perf.log"s, const std::string& async_log_file_path = "logs/bsp_perf_async.log"s);
    virtual ~BspLogger();

    enum class LogLevel {
        Debug,
        Info,
        Warn,
        Critical,
        Error
    };
    BspLogger(const BspLogger&) = delete;
    BspLogger(BspLogger&&) = delete;
    BspLogger& operator=(const BspLogger&) = delete;
    BspLogger& operator=(BspLogger&&) = delete;

    void setPattern(const std::string& pattern = "[%H:%M:%S.%f][%^%l%$] %v"s)
    {
        // Set the pattern for the loggers
        spdlog::set_pattern(pattern);
    }

    template<typename... Args>
    void printStdoutLog(LogLevel level, std::string_view sv, const Args &... args)
    {
        // Print the log message to stdout
        printLogger(m_stdout_logger, level, sv, args...);
    }

    template<typename... Args>
    void printFileLog(LogLevel level, std::string_view sv, const Args &... args)
    {
        // Print the log message to a file
        printLogger(m_file_logger, level, sv, args...);
    }


    template<typename... Args>
    void printAsyncFileLog(LogLevel level, std::string_view sv, const Args &... args)
    {
        // Print the log message to a file asynchronously
        printLogger(m_async_file_logger, level, sv, args...);
    }

protected:
    template<typename... Args>
    void printLogger(std::shared_ptr<spdlog::logger> &logger, LogLevel level, std::string_view sv, const Args &... args)
    {
        // Print the log message to the logger
        switch (level)
        {
        case LogLevel::Info:
            logger->info(sv, args...);
            break;
        case LogLevel::Debug:
            logger->debug(sv, args...);
            break;
        case LogLevel::Warn:
            logger->warn(sv, args...);
            break;
        case LogLevel::Critical:
            logger->critical(sv, args...);
            break;
        case LogLevel::Error:
            logger->error(sv, args...);
            break;
        default:
            break;
        }
    }

private:
    // Your code here
    std::shared_ptr<spdlog::logger> m_stdout_logger;
    std::shared_ptr<spdlog::logger> m_file_logger;
    std::shared_ptr<spdlog::logger> m_async_file_logger;
};


} // namespace shared
} // namespace bsp_perf

#endif // BSP_LOGGER_HPP