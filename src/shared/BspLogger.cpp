#include "BspLogger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <chrono>

namespace bsp_perf {
namespace shared {

    BspLogger::BspLogger(const std::string& logger_id, const std::string& log_file_path, const std::string& async_log_file_path)
    {
        auto logger_name = logger_id + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        m_stdout_logger = spdlog::stdout_color_mt(logger_name);
        logger_name = logger_id + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        m_file_logger = spdlog::basic_logger_mt(logger_name, log_file_path);
        logger_name = logger_id + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        m_async_file_logger = spdlog::basic_logger_mt<spdlog::async_factory>(logger_name, async_log_file_path);

        // Initialize the logger here
        m_stdout_logger->set_level(spdlog::level::debug);

        m_file_logger->set_level(spdlog::level::info);
        // Set the flush level for the file logger
        m_file_logger->flush_on(spdlog::level::info);

        m_async_file_logger->set_level(spdlog::level::info);
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

} // namespace shared
} // namespace bsp_perf