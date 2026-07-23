#ifndef __SVS_BLENDER_HPP__
#define __SVS_BLENDER_HPP__

#include "SvsTypes.hpp"
#include <array>
#include <opencv2/core.hpp>

namespace bsp_perf
{
namespace svs
{

class SvsBlender
{
public:
    bool setup(const SurroundViewConfig& config);
    bool blend(const std::array<cv::Mat, kCameraCount>& warpedImages,
               const std::array<cv::Mat, kCameraCount>& weights,
               cv::Mat& output) const;
    void reset();

private:
    cv::Point vehicleToBev(double x, double y) const;
    void drawVehicle(cv::Mat& image) const;

private:
    SurroundViewConfig m_config;
    cv::Mat m_vehicleImage;
    bool m_ready{false};
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_BLENDER_HPP__
