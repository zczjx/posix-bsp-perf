#ifndef __FUSION_GATING_HPP__
#define __FUSION_GATING_HPP__

namespace bsp_perf
{
namespace fusion
{

// 不做门限(永远接受)
constexpr double kNoGate = -1.0;

// 卡方分布 99% 分位数, 下标为自由度(观测维度) 1..6
inline double chi2Gate99(int dof)
{
    static const double kTable[] = {0.0, 6.635, 9.210, 11.345, 13.277, 15.086, 16.812};
    return (dof >= 1 && dof <= 6) ? kTable[dof] : kNoGate;
}

// 马氏距离平方 (即 NIS) 是否在门限内
inline bool passGate(double nis, double gate)
{
    return gate < 0.0 || nis <= gate;
}

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_GATING_HPP__
