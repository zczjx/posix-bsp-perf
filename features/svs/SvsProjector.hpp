#ifndef __SVS_PROJECTOR_HPP__
#define __SVS_PROJECTOR_HPP__

#include "SvsTypes.hpp"
#include <opencv2/core.hpp>

namespace bsp_perf
{
namespace svs
{

class SvsProjector
{
public:
    cv::Size projectSize(const std::string& cameraName, const ProjectionLayout& layout) const;
    bool project(const cv::Mat& src,
                 const CameraParameters& params,
                 const CameraConfig& cameraConfig,
                 const ProjectionLayout& layout,
                 cv::Mat& dst) const;

private:
    bool undistortByRemap(const cv::Mat& src, const CameraParameters& params, cv::Mat& dst) const;
    void applyFlip(const std::string& flipMode, cv::Mat& image) const;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_PROJECTOR_HPP__
