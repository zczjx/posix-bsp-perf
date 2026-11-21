#ifndef __FUSION_GNSS_POSITION_MEAS_HPP__
#define __FUSION_GNSS_POSITION_MEAS_HPP__

#include <Eigen/Dense>

#include "core/Angle.hpp"
#include "models/motion/ImuPlanarMotion.hpp"

namespace bsp_perf
{
namespace fusion
{

// GNSS 位置观测 z = [px, py], 对应 ekf.py 的 update_gnss()
class GnssPositionMeas
{
public:
    using StateVec = Eigen::Matrix<double, kStateDim, 1>;
    using MeasVec = Eigen::Matrix<double, 2, 1>;
    using MeasMat = Eigen::Matrix<double, 2, 2>;
    using MeasJac = Eigen::Matrix<double, 2, kStateDim>;

    explicit GnssPositionMeas(double sigma_m) : sigma_(sigma_m) {}

    void evaluate(const StateVec& x, MeasVec& z_pred, MeasJac& H, MeasMat& R) const
    {
        z_pred << x[kPx], x[kPy];
        H.setZero();
        H(0, kPx) = 1.0;
        H(1, kPy) = 1.0;
        R = MeasMat::Identity() * sigma_ * sigma_;
    }

    MeasVec residual(const MeasVec& z, const MeasVec& z_pred) const { return z - z_pred; }

    void normalize(StateVec& x) const { x[kYaw] = wrapToPi(x[kYaw]); }

private:
    double sigma_;
};

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_GNSS_POSITION_MEAS_HPP__
