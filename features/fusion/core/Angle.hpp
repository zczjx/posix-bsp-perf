#ifndef __FUSION_ANGLE_HPP__
#define __FUSION_ANGLE_HPP__

#include <cmath>

namespace bsp_perf
{
namespace fusion
{

constexpr double kPi = 3.14159265358979323846;

// 归一化到 [-pi, pi), 与 Python 的 (a + pi) % (2 pi) - pi 等价
inline double wrapToPi(double a)
{
    double r = std::fmod(a + kPi, 2.0 * kPi);
    if (r < 0.0)
    {
        r += 2.0 * kPi;
    }
    return r - kPi;
}

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_ANGLE_HPP__
