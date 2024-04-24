#include "BspLogger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

namespace bsp_perf {
namespace shared {

    BspLogger::BspLogger() {
        // Initialize the logger here
        m_stdout_logger = spdlog::stdout_color_mt("console", spdlog::color_mode::automatic);
        m_stdout_logger->set_level(spdlog::level::debug);

        m_file_logger = spdlog::basic_logger_mt("file_logger", "logs/bsp_perf.log");
        m_file_logger->set_level(spdlog::level::info);

        m_async_file_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "logs/bsp_perf_async.log");
        m_async_file_logger->set_level(spdlog::level::info);

        // Set the pattern for log messages
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        // Set the flush level for the file logger
        m_file_logger->flush_on(spdlog::level::info);

        // Set the flush level for the async file logger
        m_async_file_logger->flush_on(spdlog::level::info);
    }

    BspLogger::~BspLogger() {
        // Cleanup the logger here
        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::shutdown();
        m_stdout_logger.reset();
        m_file_logger.reset();
        m_async_file_logger.reset();
    }

    // template<typename... Args>
    // void BspLogger::printFileLog(LogLevel level, string_view_t fmt, const Args &... args)
    // {
    //     // Print the log message to a file
    //     printLogger(m_file_logger, level, fmt, args...);
    // }

    // template<typename... Args>
    // void BspLogger::printAsyncFileLog(LogLevel level, string_view_t fmt, const Args &... args)
    // {
    //     // Print the log message to a file asynchronously
    //     printLogger(m_async_file_logger, level, fmt, args...);
    // }


} // namespace shared
} // namespace bsp_perf