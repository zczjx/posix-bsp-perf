// GNSS+IMU EKF 回放 sample: 读 record_csv.py 录的 CSV, 调用 IGnssImuLocalizer,
// 输出结果 CSV (列与 ekf_offline.py 一致) 和 PNG。
//   fusion_replay <prefix> [--out <prefix>] [--gnss-std v] [--gyro-std v] [--accel-std v]
//                 [--bias-walk-std v] [--gnss-every n] [--skip-imu n] [--outage s e]
//                 [--origin-lat v] [--origin-lon v] [--gate chi2]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TrajectoryPlot.hpp"
#include "fusion/core/Angle.hpp"
#include "fusion/geo/GeoConverter.hpp"
#include "fusion/localization/IGnssImuLocalizer.hpp"

using namespace bsp_perf::fusion;

namespace
{

struct Truth
{
    std::vector<double> t, x, y, yaw, v;

    // 线性插值, 与 np.interp 一致 (区间外取端点值)
    double interp(const std::vector<double>& col, double q) const
    {
        if (q <= t.front()) return col.front();
        if (q >= t.back()) return col.back();
        const size_t i = std::upper_bound(t.begin(), t.end(), q) - t.begin();
        const double w = (q - t[i - 1]) / (t[i] - t[i - 1]);
        return col[i - 1] + w * (col[i] - col[i - 1]);
    }
};

struct SensorRow
{
    double t, a, b;
    bool is_imu;
};

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        out.push_back(tok);
    }
    return out;
}

bool loadSensors(const std::string& path, std::vector<SensorRow>& imu, std::vector<SensorRow>& gnss)
{
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    std::getline(f, line); // 表头: t,type,a,b,c,d
    while (std::getline(f, line))
    {
        const auto c = split(line);
        if (c.size() < 4) continue;
        SensorRow r{std::stod(c[0]), std::stod(c[2]), std::stod(c[3]), c[1] == "imu"};
        (r.is_imu ? imu : gnss).push_back(r);
    }
    return true;
}

bool loadTruth(const std::string& path, Truth& tr)
{
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line))
    {
        const auto c = split(line);
        if (c.size() < 5) continue;
        tr.t.push_back(std::stod(c[0]));
        tr.x.push_back(std::stod(c[1]));
        tr.y.push_back(std::stod(c[2]));
        tr.yaw.push_back(std::stod(c[3]));
        tr.v.push_back(std::stod(c[4]));
    }
    return !tr.t.empty();
}

double wrap(double a) { return wrapToPi(a); }

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "用法: %s <prefix> [选项], 见源码头部注释\n", argv[0]);
        return 1;
    }
    const std::string prefix = argv[1];
    std::string out = prefix + "_cpp";
    LocalizerConfig cfg;
    double outage_s = -1.0, outage_e = -1.0;

    for (int i = 2; i < argc; ++i)
    {
        const std::string k = argv[i];
        auto next = [&](int n) { return (i + n < argc) ? argv[i + n] : "0"; };
        if (k == "--out") out = next(1), ++i;
        else if (k == "--gnss-std") cfg.gnss_std = std::atof(next(1)), ++i;
        else if (k == "--gyro-std") cfg.gyro_std = std::atof(next(1)), ++i;
        else if (k == "--accel-std") cfg.accel_std = std::atof(next(1)), ++i;
        else if (k == "--bias-walk-std") cfg.gyro_bias_walk_std = std::atof(next(1)), ++i;
        else if (k == "--gnss-every") cfg.gnss_every = std::atoi(next(1)), ++i;
        else if (k == "--skip-imu") cfg.skip_imu = std::atoi(next(1)), ++i;
        else if (k == "--origin-lat") cfg.origin_lat = std::atof(next(1)), ++i;
        else if (k == "--origin-lon") cfg.origin_lon = std::atof(next(1)), ++i;
        else if (k == "--gate") cfg.gate_chi2 = std::atof(next(1)), ++i;
        else if (k == "--outage") outage_s = std::atof(next(1)), outage_e = std::atof(next(2)), i += 2;
        else { std::fprintf(stderr, "未知参数: %s\n", k.c_str()); return 1; }
    }

    std::vector<SensorRow> imu, gnss;
    Truth truth;
    if (!loadSensors(prefix + "_sensors.csv", imu, gnss) || !loadTruth(prefix + "_truth.csv", truth))
    {
        std::fprintf(stderr, "读取 %s_sensors.csv / _truth.csv 失败\n", prefix.c_str());
        return 1;
    }
    std::printf("读入: imu %zu, gnss %zu, truth %zu\n", imu.size(), gnss.size(), truth.t.size());

    // 初始航向取真值 (与 ekf_offline.py 一致): 以第一帧可用 IMU 时刻的真值为准
    const size_t first = std::min<size_t>(cfg.skip_imu, imu.size() - 1);
    cfg.initial_yaw = truth.interp(truth.yaw, imu[first].t);
    auto loc = IGnssImuLocalizer::create(cfg);
    GeoConverter geo(cfg.origin_lat, cfg.origin_lon);

    std::ofstream csv(out + ".csv");
    csv << "t,gt_x,gt_y,gt_yaw,gt_v,gnss_x,gnss_y,est_x,est_y,est_yaw,est_v,est_bg,est_ba,pos_sigma,nis\n";
    std::vector<PlotRow> plot;
    double sp = 0, sg = 0, sy = 0, sv = 0;
    double gx = 0, gy = 0;
    size_t gi = 0, n = 0;
    long last_used = 0;
    bool have_gnss = false;
    char buf[512];

    for (size_t k = 0; k < imu.size(); ++k)
    {
        const double t = imu[k].t;
        // 先喂时间戳 <= t 的 GNSS; 停电窗口内的帧标记为无效
        while (gi < gnss.size() && gnss[gi].t <= t + 1e-6)
        {
            GnssSample g;
            g.t = gnss[gi].t;
            g.lat = gnss[gi].a;
            g.lon = gnss[gi].b;
            g.valid = !(outage_s >= 0.0 && t >= outage_s && t <= outage_e);
            loc->feedGnss(g);
            geo.toLocal(g.lat, g.lon, gx, gy);
            have_gnss = true;
            ++gi;
        }
        const Status st = loc->feedImu({t, imu[k].a, imu[k].b});
        const Pose2D p = loc->pose();
        // 只在真正做过一次预测后才输出 (初始化那一帧不输出, 与 ekf_offline.py 一致)
        const long used = loc->diagnostics().imu_used;
        const bool predicted = used > last_used;
        last_used = used;
        if (st != Status::Ok || !p.valid || !have_gnss || !predicted)
        {
            continue;
        }

        const double tx = truth.interp(truth.x, t), ty = truth.interp(truth.y, t);
        const double tyaw = truth.interp(truth.yaw, t), tv = truth.interp(truth.v, t);
        const Diagnostics d = loc->diagnostics();
        std::snprintf(buf, sizeof(buf), "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                      t, tx, ty, tyaw, tv, gx, gy, p.x, p.y, p.yaw, p.v, p.bg, p.ba, p.pos_sigma, d.last_nis);
        csv << buf;
        plot.push_back({t, tx, ty, gx, gy, p.x, p.y});

        sp += std::pow(p.x - tx, 2) + std::pow(p.y - ty, 2);
        sg += std::pow(gx - tx, 2) + std::pow(gy - ty, 2);
        sy += std::pow(wrap(p.yaw - tyaw), 2);
        sv += std::pow(p.v - tv, 2);
        ++n;
    }
    csv.close();

    if (n == 0)
    {
        std::fprintf(stderr, "没有产生任何输出行\n");
        return 2;
    }
    const double kRad2Deg = 180.0 / kPi;
    const Diagnostics d = loc->diagnostics();
    std::printf("已保存 %s.csv (%zu 行)\n", out.c_str(), n);
    std::printf("位置 RMSE  EKF %.3f m | 纯 GNSS %.3f m\n", std::sqrt(sp / n), std::sqrt(sg / n));
    std::printf("航向 RMSE  %.2f deg\n", std::sqrt(sy / n) * kRad2Deg);
    std::printf("速度 RMSE  %.3f m/s | 末尾陀螺零偏估计 %.4f rad/s\n", std::sqrt(sv / n), loc->pose().bg);
    std::printf("GNSS 更新 接受 %ld / 拒绝 %ld | 平均 NIS %.2f\n", d.gnss_accepted, d.gnss_rejected, d.mean_nis);

    char title[128];
    std::snprintf(title, sizeof(title), "RMSE EKF %.2f m | GNSS %.2f m | yaw %.1f deg",
                  std::sqrt(sp / n), std::sqrt(sg / n), std::sqrt(sy / n) * kRad2Deg);
    if (plotTrajectory(plot, title, out + ".png"))
    {
        std::printf("已保存 %s.png\n", out.c_str());
    }
    return 0;
}
