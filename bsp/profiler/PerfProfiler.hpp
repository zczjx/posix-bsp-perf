#ifndef PERF_PROFILER_HPP
#define PERF_PROFILER_HPP

#include <string>
#include <chrono>
#include <shared/BspLogger.hpp>

namespace bsp_perf {
namespace common {

using time_point_t = std::chrono::steady_clock::time_point;
using namespace std::string_literals;

constexpr int32_t memValueMB(size_t size) { return (size << 20); }


class PerfProfiler {
public:
    static constexpr char LOG_TAG[] {"[PerfProfiler]: "};
    PerfProfiler(std::string& caseName, const std::string& profile_file_path = "logs/perf_case.profile"s);
    ~PerfProfiler() = default;

    PerfProfiler(const PerfProfiler&) = delete;
    PerfProfiler& operator=(const PerfProfiler&) = delete;

    PerfProfiler(PerfProfiler&&) = delete;
    PerfProfiler& operator=(PerfProfiler&&) = delete;

    std::string& getCaseName() { return m_caseName; }

    template<typename T>
    void asyncRecordPerfData(const std::string& metricName, T val, const std::string& unitName)
    {
        m_logger->printAsyncFileLog(bsp_perf::shared::BspLogger::LogLevel::Warn,
            "{}:{}:{}:{}", m_caseName, metricName, val, unitName);
    }

    template<typename T>
    void printPerfData(const std::string& metricName, T val, const std::string& unitName)
    {
        m_logger->printStdoutLog(bsp_perf::shared::BspLogger::LogLevel::Warn,
            "{}:{}:{}:{}", m_caseName, metricName, val, unitName);
    }

    time_point_t getCurrentTimePoint()
    {
       return std::chrono::steady_clock::now();
    }

    time_t getLatencyUs(time_point_t& start, time_point_t& end)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

private:
    std::string m_caseName;
    std::unique_ptr<bsp_perf::shared::BspLogger> m_logger;
    // Add your member functions and variables here
};

} // namespace common
} // namespace bsp_perf

#endif // PERF_PROFILER_HPP