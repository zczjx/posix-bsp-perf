#ifndef PERF_PROFILER_HPP
#define PERF_PROFILER_HPP


namespace bsp_perf {
namespace common {

#include <shared/BspLogger.hpp>

class PerfProfiler {
public:
    PerfProfiler() = default;
    ~PerfProfiler() = default;

    PerfProfiler(const PerfProfiler&) = delete;
    PerfProfiler& operator=(const PerfProfiler&) = delete;

    PerfProfiler(PerfProfiler&&) = delete;
    PerfProfiler& operator=(PerfProfiler&&) = delete;

    // Add your member functions and variables here
};

} // namespace common
} // namespace bsp_perf

#endif // PERF_PROFILER_HPP