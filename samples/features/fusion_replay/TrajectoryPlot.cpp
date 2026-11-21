#include "TrajectoryPlot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{

const cv::Scalar kBlack(0, 0, 0);
const cv::Scalar kRed(60, 60, 230);
const cv::Scalar kBlue(230, 80, 40);
const cv::Scalar kGray(200, 200, 200);

// 把世界坐标映射到画布矩形内, 保持等比例, y 轴向上
struct Viewport
{
    cv::Rect area;
    double min_x, min_y, scale;

    cv::Point map(double x, double y) const
    {
        return cv::Point(area.x + static_cast<int>((x - min_x) * scale),
                         area.y + area.height - static_cast<int>((y - min_y) * scale));
    }
};

Viewport fitViewport(const cv::Rect& area, double min_x, double max_x, double min_y, double max_y)
{
    const double w = std::max(max_x - min_x, 1e-6);
    const double h = std::max(max_y - min_y, 1e-6);
    Viewport v;
    v.area = area;
    v.scale = std::min(area.width / w, area.height / h) * 0.95;
    v.min_x = min_x - (area.width / v.scale - w) / 2.0;
    v.min_y = min_y - (area.height / v.scale - h) / 2.0;
    return v;
}

void polyline(cv::Mat& img, const Viewport& vp, const std::vector<PlotRow>& rows,
              double PlotRow::*px, double PlotRow::*py, const cv::Scalar& color, int thick)
{
    for (size_t i = 1; i < rows.size(); ++i)
    {
        cv::line(img, vp.map(rows[i - 1].*px, rows[i - 1].*py), vp.map(rows[i].*px, rows[i].*py),
                 color, thick, cv::LINE_AA);
    }
}

void drawLegend(cv::Mat& img, int x, int y, const char* text, const cv::Scalar& color)
{
    cv::line(img, cv::Point(x, y), cv::Point(x + 24, y), color, 3, cv::LINE_AA);
    cv::putText(img, text, cv::Point(x + 32, y + 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, kBlack, 1, cv::LINE_AA);
}

} // namespace

bool plotTrajectory(const std::vector<PlotRow>& rows, const std::string& title, const std::string& png_path)
{
    if (rows.size() < 2)
    {
        return false;
    }

    cv::Mat img(560, 1280, CV_8UC3, cv::Scalar(255, 255, 255));
    const cv::Rect left(30, 50, 600, 480);
    const cv::Rect right(690, 50, 560, 480);
    cv::rectangle(img, left, kGray, 1);
    cv::rectangle(img, right, kGray, 1);

    // --- 左图: 轨迹 ---
    double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18;
    for (const auto& r : rows)
    {
        for (double x : {r.gt_x, r.gnss_x, r.est_x}) { minx = std::min(minx, x); maxx = std::max(maxx, x); }
        for (double y : {r.gt_y, r.gnss_y, r.est_y}) { miny = std::min(miny, y); maxy = std::max(maxy, y); }
    }
    const Viewport vp = fitViewport(left, minx, maxx, miny, maxy);
    for (const auto& r : rows)
    {
        cv::circle(img, vp.map(r.gnss_x, r.gnss_y), 1, kRed, -1, cv::LINE_AA);
    }
    polyline(img, vp, rows, &PlotRow::gt_x, &PlotRow::gt_y, kBlack, 2);
    polyline(img, vp, rows, &PlotRow::est_x, &PlotRow::est_y, kBlue, 2);

    // --- 右图: 位置误差 ---
    std::vector<double> e_gnss, e_est;
    double emax = 1e-6;
    for (const auto& r : rows)
    {
        e_gnss.push_back(std::hypot(r.gnss_x - r.gt_x, r.gnss_y - r.gt_y));
        e_est.push_back(std::hypot(r.est_x - r.gt_x, r.est_y - r.gt_y));
        emax = std::max({emax, e_gnss.back(), e_est.back()});
    }
    const double t0 = rows.front().t, t1 = rows.back().t;
    auto mapErr = [&](double t, double e) {
        return cv::Point(right.x + static_cast<int>((t - t0) / (t1 - t0) * right.width),
                         right.y + right.height - static_cast<int>(e / emax * right.height * 0.95));
    };
    for (size_t i = 1; i < rows.size(); ++i)
    {
        cv::line(img, mapErr(rows[i - 1].t, e_gnss[i - 1]), mapErr(rows[i].t, e_gnss[i]), kRed, 1, cv::LINE_AA);
        cv::line(img, mapErr(rows[i - 1].t, e_est[i - 1]), mapErr(rows[i].t, e_est[i]), kBlue, 2, cv::LINE_AA);
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "max %.1f m", emax);
    cv::putText(img, buf, cv::Point(right.x + 6, right.y + 16), cv::FONT_HERSHEY_SIMPLEX, 0.45, kBlack, 1, cv::LINE_AA);
    std::snprintf(buf, sizeof(buf), "t: %.0f - %.0f s", t0, t1);
    cv::putText(img, buf, cv::Point(right.x + 6, right.y + right.height - 6), cv::FONT_HERSHEY_SIMPLEX, 0.45, kBlack, 1, cv::LINE_AA);

    cv::putText(img, title, cv::Point(30, 28), cv::FONT_HERSHEY_SIMPLEX, 0.6, kBlack, 1, cv::LINE_AA);
    drawLegend(img, 700, 30, "GNSS raw", kRed);
    drawLegend(img, 820, 30, "EKF", kBlue);
    drawLegend(img, 900, 30, "truth (left)", kBlack);

    return cv::imwrite(png_path, img);
}
