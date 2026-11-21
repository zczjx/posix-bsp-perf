#include "IGnssImuLocalizer.hpp"

#include "core/ExtendedKalmanFilter.hpp"
#include "geo/GeoConverter.hpp"
#include "models/measurement/GnssPositionMeas.hpp"
#include "models/motion/ImuPlanarMotion.hpp"

namespace bsp_perf
{
namespace fusion
{

namespace
{

class GnssImuLocalizer : public IGnssImuLocalizer
{
public:
    explicit GnssImuLocalizer(const LocalizerConfig& cfg)
        : cfg_(cfg),
          motion_(ImuNoise{cfg.gyro_std, cfg.accel_std, cfg.gyro_bias_walk_std,
                           cfg.accel_bias_walk_std, 0.02}),
          gnss_meas_(cfg.gnss_std)
    {
        if (cfg_.use_fixed_origin)
        {
            geo_.setOrigin(cfg_.origin_lat, cfg_.origin_lon);
            origin_ready_ = true;
        }
    }

    Status feedGnss(const GnssSample& s) override
    {
        ++diag_.gnss_received;
        if (!origin_ready_)
        {
            geo_.setOrigin(s.lat, s.lon);
            origin_ready_ = true;
        }
        double x = 0.0, y = 0.0;
        geo_.toLocal(s.lat, s.lon, x, y);
        last_gnss_ << x, y;
        has_gnss_ = true;
        ++gnss_seq_;

        // EKF 还没起来时只记录最新值, 不更新 (与 Python 一致)
        if (!inited_)
        {
            return Status::NotInitialized;
        }
        if (!s.valid || (cfg_.gnss_every > 1 && (gnss_seq_ % cfg_.gnss_every) != 0))
        {
            return Status::Skipped;
        }

        const UpdateResult r = ekf_.update(gnss_meas_, last_gnss_, cfg_.gate_chi2);
        if (r.accepted)
        {
            ++diag_.gnss_accepted;
            return Status::Ok;
        }
        ++diag_.gnss_rejected;
        return Status::Skipped;
    }

    Status feedImu(const ImuSample& s) override
    {
        // 开头几帧 IMU 带异常值, 直接丢弃 (按到达序号计)
        if (imu_seen_++ < cfg_.skip_imu)
        {
            ++diag_.imu_skipped;
            return Status::Skipped;
        }
        if (!has_gnss_)
        {
            return Status::NotInitialized;
        }

        if (!inited_)
        {
            ImuPlanarMotion::StateVec x0;
            x0 << last_gnss_[0], last_gnss_[1], cfg_.initial_yaw, 0.0, 0.0, 0.0;
            ImuPlanarMotion::StateMat P0 = ImuPlanarMotion::StateMat::Zero();
            const double pv = cfg_.gnss_std * cfg_.init_pos_var_scale;
            P0(kPx, kPx) = pv * pv;
            P0(kPy, kPy) = pv * pv;
            P0(kYaw, kYaw) = cfg_.init_yaw_var;
            P0(kV, kV) = cfg_.init_v_var;
            P0(kBg, kBg) = cfg_.init_bg_var;
            P0(kBa, kBa) = cfg_.init_ba_var;
            ekf_.init(x0, P0);
            last_t_ = s.t;
            t_ = s.t;
            inited_ = true;
            return Status::Ok;
        }

        const double dt = s.t - last_t_;
        if (dt <= 0.0)
        {
            return Status::BadTimestamp; // 乱序或重复, 不更新 last_t_
        }
        last_t_ = s.t;
        if (dt > cfg_.max_dt)
        {
            ++diag_.imu_skipped;
            return Status::Skipped;
        }

        ekf_.predict(motion_, ImuInput{s.gyro_z, s.accel_x}, dt);
        t_ = s.t;
        ++diag_.imu_used;
        return Status::Ok;
    }

    Pose2D pose() const override
    {
        Pose2D p;
        if (!inited_)
        {
            return p;
        }
        const auto& x = ekf_.state();
        const auto& P = ekf_.covariance();
        p.t = t_;
        p.x = x[kPx];
        p.y = x[kPy];
        p.yaw = x[kYaw];
        p.v = x[kV];
        p.bg = x[kBg];
        p.ba = x[kBa];
        p.pos_sigma = std::sqrt(P(kPx, kPx) + P(kPy, kPy));
        p.valid = true;
        return p;
    }

    Diagnostics diagnostics() const override
    {
        Diagnostics d = diag_;
        d.last_nis = ekf_.stats().last_nis;
        d.mean_nis = ekf_.stats().mean();
        return d;
    }

private:
    LocalizerConfig cfg_;
    ImuPlanarMotion motion_;
    GnssPositionMeas gnss_meas_;
    ExtendedKalmanFilter<kStateDim> ekf_;
    GeoConverter geo_;
    Diagnostics diag_;

    GnssPositionMeas::MeasVec last_gnss_ = GnssPositionMeas::MeasVec::Zero();
    bool origin_ready_ = false;
    bool has_gnss_ = false;
    bool inited_ = false;
    long gnss_seq_ = 0;
    int imu_seen_ = 0;
    double last_t_ = 0.0;
    double t_ = 0.0;
};

} // namespace

std::unique_ptr<IGnssImuLocalizer> IGnssImuLocalizer::create(const LocalizerConfig& config)
{
    return std::unique_ptr<IGnssImuLocalizer>(new GnssImuLocalizer(config));
}

} // namespace fusion
} // namespace bsp_perf
