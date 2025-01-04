#include "PerfProfiler.hpp"

namespace bsp_perf {
namespace common {

using namespace bsp_perf::shared;

constexpr char PerfProfiler::LOG_TAG[];

PerfProfiler::PerfProfiler(std::string& caseName, const std::string& profile_file_path):
    m_caseName{caseName},
    m_logger{std::make_unique<BspLogger>("PerfProfiler", "logs/bsp_perf.log"s, profile_file_path)}
{
    // Add your code here
    m_logger->setPattern("[%H:%M:%S.%f][thread:%t][%v]"s);
}

}   // namespace common

}// namespace bsp_perf