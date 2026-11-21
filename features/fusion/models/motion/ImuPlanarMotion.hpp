#ifndef __FUSION_IMU_PLANAR_MOTION_HPP__
#define __FUSION_IMU_PLANAR_MOTION_HPP__

#include <Eigen/Dense>

#include "core/Angle.hpp"

namespace bsp_perf
{
namespace fusion
{

// 状态下标: [px, py, yaw, v, bg, ba]
enum StateIdx
{
    kPx = 0,
    kPy = 1,
    kYaw = 2,
    kV = 3,
    kBg = 4,
    kBa = 5,
    kStateDim = 6
};

// 运动模型的控制量: IMU 读数 (ROS/ENU 约定, 与 Python 一致)
struct ImuInput
{
    double gyro_z = 0.0;  // rad/s
    double accel_x = 0.0; // m/s^2, 车头方向
};

struct ImuNoise
{
    double gyro_std = 0.01;
    double accel_std = 0.1;
    double gyro_bias_walk_std = 1e-3;
    double accel_bias_walk_std = 5e-3;
    double pos_model_std = 0.02; // 位置的模型误差 (m/s^0.5 量级, 与 Python 的 0.02*dt 对应)
};

// 平面 IMU 机械编排: IMU 作为控制量输入, 对应 ekf.py 的 predict()
class ImuPlanarMotion
{
public:
    using StateVec = Eigen::Matrix<double, kStateDim, 1>;
    using StateMat = Eigen::Matrix<double, kStateDim, kStateDim>;
    using Control = ImuInput;

    explicit ImuPlanarMotion(const ImuNoise& noise) : noise_(noise) {}

    void propagate(const StateVec& x, const Control& u, double dt,
                   StateVec& x_next, StateMat& F, StateMat& Q) const
    {
        const double yaw = x[kYaw];
        const double v = x[kV];
        const double w = u.gyro_z - x[kBg];
        const double c = std::cos(yaw);
        const double s = std::sin(yaw);

        x_next[kPx] = x[kPx] + v * c * dt;
        x_next[kPy] = x[kPy] + v * s * dt;
        x_next[kYaw] = wrapToPi(yaw + w * dt);
        x_next[kV] = v + (u.accel_x - x[kBa]) * dt;
        x_next[kBg] = x[kBg];
        x_next[kBa] = x[kBa];

        F.setIdentity();
        F(kPx, kYaw) = -v * s * dt;
        F(kPx, kV) = c * dt;
        F(kPy, kYaw) = v * c * dt;
        F(kPy, kV) = s * dt;
        F(kYaw, kBg) = -dt;
        F(kV, kBa) = -dt;

        Q.setZero();
        Q(kPx, kPx) = sq(noise_.pos_model_std * dt);
        Q(kPy, kPy) = sq(noise_.pos_model_std * dt);
        Q(kYaw, kYaw) = sq(noise_.gyro_std * dt);
        Q(kV, kV) = sq(noise_.accel_std * dt);
        Q(kBg, kBg) = sq(noise_.gyro_bias_walk_std * dt);
        Q(kBa, kBa) = sq(noise_.accel_bias_walk_std * dt);
    }

private:
    static double sq(double a) { return a * a; }
    ImuNoise noise_;
};

} // namespace fusion
} // namespace bsp_perf

#endif // __FUSION_IMU_PLANAR_MOTION_HPP__
