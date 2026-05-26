#ifndef __SVS_CALIBRATOR_HPP__
#define __SVS_CALIBRATOR_HPP__

#include "SvsTypes.hpp"
#include <opencv2/core.hpp>

namespace bsp_perf
{
namespace svs
{

class SvsCalibrator
{
public:
    std::vector<cv::Point2f> defaultProjectKeypoints(const std::string& cameraName,
                                                     const ProjectionLayout& layout) const;
    bool computeProjectMatrix(const std::string& cameraName,
                              const std::vector<cv::Point2f>& imagePoints,
                              const ProjectionLayout& layout,
                              cv::Mat& projectMatrix) const;
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_CALIBRATOR_HPP__
