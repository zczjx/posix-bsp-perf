#include "PerfProfiler.hpp"

namespace bsp_perf {
namespace common {

constexpr char PerfProfiler::LOG_TAG[];

PerfProfiler::PerfProfiler(std::string& caseName):
    m_caseName{caseName}, m_logger{std::make_unique<bsp_perf::shared::BspLogger>()}
{
    // Add your code here
}

}   // namespace common

}// namespace bsp_perf