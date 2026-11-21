#ifndef __FUSION_TRAJECTORY_PLOT_HPP__
#define __FUSION_TRAJECTORY_PLOT_HPP__

#include <string>
#include <vector>

struct PlotRow
{
    double t;
    double gt_x, gt_y;
    double gnss_x, gnss_y;
    double est_x, est_y;
};

// 画三幅图: 轨迹(ENU)、位置误差随时间、(右侧留作文字摘要), 保存为 PNG
bool plotTrajectory(const std::vector<PlotRow>& rows, const std::string& title, const std::string& png_path);

#endif // __FUSION_TRAJECTORY_PLOT_HPP__
