#ifndef __BSP_TIME_UTILS_HPP__
#define __BSP_TIME_UTILS_HPP__

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace bsp_perf {
namespace shared {

class BspTimeUtils
{
public:

    static std::string getCurrentTimeString()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (tm.tm_year + 1900)
            << std::setw(2) << (tm.tm_mon + 1)
            << std::setw(2) << tm.tm_mday << "_"
            << std::setw(2) << tm.tm_hour
            << std::setw(2) << tm.tm_min
            << std::setw(2) << tm.tm_sec;

        return oss.str();
    }

};
} // namespace shared
} // namespace bsp_perf

#endif // __BSP_TIME_UTILS_HPP__