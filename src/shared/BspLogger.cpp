#include "BspLogger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

namespace bsp_perf {
namespace shared {

    BspLogger::BspLogger(const std::string& log_file_path, const std::string& async_log_file_path):
        m_stdout_logger {spdlog::stdout_color_mt("stdio")},
        m_file_logger {spdlog::basic_logger_mt("logger", log_file_path)},
        m_async_file_logger {spdlog::basic_logger_mt<spdlog::async_factory>("async_logger", async_log_file_path)}
    {
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