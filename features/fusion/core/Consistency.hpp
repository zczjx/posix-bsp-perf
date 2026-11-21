#ifndef __FUSION_CONSISTENCY_HPP__
#define __FUSION_CONSISTENCY_HPP__

#include <limits>

namespace bsp_perf
{
namespace fusion
{

// NIS 累计统计: 一致的滤波器, 平均 NIS 应接近观测维度
struct NisStats
{
    long accepted = 0;
    long rejected = 0;
    double nis_sum = 0.0;
    double last_nis = std::numeric_limits<double>::quiet_NaN(); // 尚无更新时为 NaN

    void add(double nis, bool ok)
    {
        last_nis = nis;
        if (ok)
        {
            ++accepted;
            nis_sum += nis;
        }
        else
        {
            ++rejected;
        }
    }

    double mean() const { return accepted > 0 ? nis_sum / static_cast<double>(accepted) : 0.0; }
};

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_CONSISTENCY_HPP__
