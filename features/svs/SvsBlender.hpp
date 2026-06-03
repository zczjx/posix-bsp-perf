#ifndef __SVS_BLENDER_HPP__
#define __SVS_BLENDER_HPP__

#include "SvsTypes.hpp"
#include <array>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace bsp_perf
{
namespace svs
{

class SvsBlender
{
public:
    bool setup(const SurroundViewConfig& config);
    void applyAwbAndLuminanceBalance(std::array<cv::Mat, kCameraCount>& images) const;
    bool blend(const std::array<cv::Mat, kCameraCount>& projectedImages, cv::Mat& output) const;
    void reset();

private:
    bool loadWeights(const std::string& path);
    bool loadVehicleImage(const std::string& path);
    void mergeImage(const cv::Mat& src1, const cv::Mat& src2, const cv::Mat& weight, cv::Mat out) const;

private:
    ProjectionLayout m_layout;
    cv::Mat m_vehicleImage;
    std::array<cv::Mat, kCameraCount> m_weights;
    bool m_hasVehicleImage{false};
    bool m_ready{false};
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_BLENDER_HPP__
