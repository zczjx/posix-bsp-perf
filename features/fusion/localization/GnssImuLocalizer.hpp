#ifndef __GNSS_IMU_LOCALIZER_HPP__
#define __GNSS_IMU_LOCALIZER_HPP__

#include <memory>

namespace bsp_perf
{
namespace fusion
{

struct ImuSample
{
    double t = 0.0;       // 秒
    double gyro_z = 0.0;  // rad/s (ROS 约定)
    double accel_x = 0.0; // m/s^2, 车头方向
};

struct GnssSample
{
    double t = 0.0;
    double lat = 0.0; // 度
    double lon = 0.0; // 度
    bool valid = true; // false: 接收机无有效定位(如隧道), 帧仍计数但不参与更新
};

struct LocalizerConfig
{
    // 观测与过程噪声 (默认值与 ekf_offline.py 一致)
    double gnss_std = 2.0;
    double gyro_std = 0.01;
    double accel_std = 0.1;
    double gyro_bias_walk_std = 1e-3;
    double accel_bias_walk_std = 5e-3;

    int gnss_every = 20;      // 每 N 个 GNSS 帧更新一次 (20Hz -> 1Hz)
    int skip_imu = 5;         // 丢弃开头 N 帧 IMU (传感器刚创建时有异常值)
    double max_dt = 0.5;      // 超过则认为数据中断, 跳过该帧预测
    double gate_chi2 = -1.0;  // <0 不做门限; 否则 NIS 超过该值则拒绝 GNSS 更新

    // GNSS 原点; use_fixed_origin=false 时取第一帧 GNSS
    bool use_fixed_origin = true;
    double origin_lat = 0.0;
    double origin_lon = 0.0;

    // 初始航向(rad)。单靠 GNSS+IMU 无法观测初始航向, 必须由外部给出
    double initial_yaw = 0.0;
    double init_pos_var_scale = 1.0; // 初始位置方差 = (gnss_std * scale)^2
    double init_yaw_var = 0.1;
    double init_v_var = 100.0;
    double init_bg_var = 0.01;
    double init_ba_var = 0.1;
};

struct Pose2D
{
    double t = 0.0;
    double x = 0.0, y = 0.0; // 局部平面坐标 (m)
    double yaw = 0.0;        // rad
    double v = 0.0;          // m/s
    double bg = 0.0;         // 陀螺零偏估计 (rad/s)
    double ba = 0.0;         // 加速度计零偏估计 (m/s^2)
    double pos_sigma = 0.0;  // sqrt(Pxx + Pyy)
    bool valid = false;      // 初始化完成前为 false
};

struct Diagnostics
{
    long imu_used = 0;
    long imu_skipped = 0;      // 开头丢弃 + dt 异常
    long gnss_received = 0;
    long gnss_accepted = 0;
    long gnss_rejected = 0;    // 被门限拒绝
    double last_nis = 0.0; // 尚无更新时为 NaN
    double mean_nis = 0.0;
};

enum class Status
{
    Ok = 0,
    NotInitialized, // 还没收到第一帧 GNSS, 先丢弃该 IMU
    Skipped,        // 按配置跳过 (开头异常帧、dt 异常、GNSS 抽样)
    BadTimestamp    // 时间戳乱序或重复
};

// GNSS + IMU 平面定位器。
// 具体类 + Pimpl: Eigen 与 EKF 内核只出现在 .cpp 里, 使用方的头文件不依赖 Eigen。
// 不可拷贝, 可移动。
class GnssImuLocalizer
{
public:
    explicit GnssImuLocalizer(const LocalizerConfig& config);
    ~GnssImuLocalizer(); // 在 .cpp 中定义 (Impl 在此处是不完整类型)

    GnssImuLocalizer(const GnssImuLocalizer&) = delete;
    GnssImuLocalizer& operator=(const GnssImuLocalizer&) = delete;
    GnssImuLocalizer(GnssImuLocalizer&&) noexcept;
    GnssImuLocalizer& operator=(GnssImuLocalizer&&) noexcept;

    // 按时间顺序喂数据。GNSS 先于同时刻 IMU 处理, 与 ekf_offline.py 一致
    Status feedGnss(const GnssSample& s);
    Status feedImu(const ImuSample& s);

    Pose2D pose() const;
    Diagnostics diagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
} // namespace bsp_perf

#endif // __GNSS_IMU_LOCALIZER_HPP__
