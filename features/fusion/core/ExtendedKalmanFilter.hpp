#ifndef __FUSION_EXTENDED_KALMAN_FILTER_HPP__
#define __FUSION_EXTENDED_KALMAN_FILTER_HPP__

#include <Eigen/Dense>

#include "Consistency.hpp"
#include "Gating.hpp"

namespace bsp_perf
{
namespace fusion
{

struct UpdateResult
{
    bool accepted = false;
    double nis = 0.0;
};

/*
 * 场景无关的 EKF 内核, 只管 x、P 和 predict/update 两个标准步骤。
 *
 * 运动模型 Motion 需要提供:
 *     using Control;                                    // 控制量 u (例如 IMU 读数)
 *     void propagate(const StateVec& x, const Control& u, double dt,
 *                    StateVec& x_next, StateMat& F, StateMat& Q) const;
 *
 * 观测模型 Meas 需要提供:
 *     using MeasVec; using MeasMat; using MeasJac; (M 维)
 *     void evaluate(const StateVec& x, MeasVec& z_pred, MeasJac& H, MeasMat& R) const;
 *     MeasVec residual(const MeasVec& z, const MeasVec& z_pred) const;   // 角度分量在这里归一化
 *     void normalize(StateVec& x) const;                                  // 更新后修正状态(如航向回卷)
 */
template <int N>
class ExtendedKalmanFilter
{
public:
    using StateVec = Eigen::Matrix<double, N, 1>;
    using StateMat = Eigen::Matrix<double, N, N>;

    void init(const StateVec& x0, const StateMat& P0)
    {
        x_ = x0;
        P_ = P0;
    }

    template <class Motion>
    void predict(const Motion& model, const typename Motion::Control& u, double dt)
    {
        StateVec x_next;
        StateMat F, Q;
        model.propagate(x_, u, dt, x_next, F, Q);
        x_ = x_next;
        P_ = F * P_ * F.transpose() + Q;
    }

    template <class Meas>
    UpdateResult update(const Meas& model, const typename Meas::MeasVec& z, double gate = kNoGate)
    {
        using MeasVec = typename Meas::MeasVec;
        using MeasMat = typename Meas::MeasMat;
        using MeasJac = typename Meas::MeasJac;
        constexpr int M = MeasVec::RowsAtCompileTime;

        MeasVec z_pred;
        MeasJac H;
        MeasMat R;
        model.evaluate(x_, z_pred, H, R);

        const MeasVec y = model.residual(z, z_pred);
        const MeasMat S = H * P_ * H.transpose() + R;
        const MeasMat S_inv = S.inverse();

        UpdateResult res;
        res.nis = y.dot(S_inv * y);
        res.accepted = passGate(res.nis, gate);
        stats_.add(res.nis, res.accepted);
        if (!res.accepted)
        {
            return res;
        }

        const Eigen::Matrix<double, N, M> K = P_ * H.transpose() * S_inv;
        x_ = x_ + K * y;
        model.normalize(x_);

        // Joseph 形式, 数值上更稳
        const StateMat I_KH = StateMat::Identity() - K * H;
        P_ = I_KH * P_ * I_KH.transpose() + K * R * K.transpose();
        return res;
    }

    const StateVec& state() const { return x_; }
    const StateMat& covariance() const { return P_; }
    const NisStats& stats() const { return stats_; }

private:
    StateVec x_ = StateVec::Zero();
    StateMat P_ = StateMat::Identity();
    NisStats stats_;
};

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_EXTENDED_KALMAN_FILTER_HPP__
