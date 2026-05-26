#include "SvsCalibrator.hpp"
#include <opencv2/imgproc.hpp>

namespace bsp_perf
{
namespace svs
{

std::vector<cv::Point2f> SvsCalibrator::defaultProjectKeypoints(const std::string& cameraName,
                                                                const ProjectionLayout& layout) const
{
    const int shiftW = layout.shiftWidth;
    const int shiftH = layout.shiftHeight;

    if (cameraName == "front" || cameraName == "back") {
        return {
            cv::Point2f(static_cast<float>(shiftW + 120), static_cast<float>(shiftH)),
            cv::Point2f(static_cast<float>(shiftW + 480), static_cast<float>(shiftH)),
            cv::Point2f(static_cast<float>(shiftW + 120), static_cast<float>(shiftH + 160)),
            cv::Point2f(static_cast<float>(shiftW + 480), static_cast<float>(shiftH + 160)),
        };
    }

    if (cameraName == "left") {
        return {
            cv::Point2f(static_cast<float>(shiftH + 280), static_cast<float>(shiftW)),
            cv::Point2f(static_cast<float>(shiftH + 840), static_cast<float>(shiftW)),
            cv::Point2f(static_cast<float>(shiftH + 280), static_cast<float>(shiftW + 160)),
            cv::Point2f(static_cast<float>(shiftH + 840), static_cast<float>(shiftW + 160)),
        };
    }

    if (cameraName == "right") {
        return {
            cv::Point2f(static_cast<float>(shiftH + 160), static_cast<float>(shiftW)),
            cv::Point2f(static_cast<float>(shiftH + 720), static_cast<float>(shiftW)),
            cv::Point2f(static_cast<float>(shiftH + 160), static_cast<float>(shiftW + 160)),
            cv::Point2f(static_cast<float>(shiftH + 720), static_cast<float>(shiftW + 160)),
        };
    }

    return {};
}

bool SvsCalibrator::computeProjectMatrix(const std::string& cameraName,
                                         const std::vector<cv::Point2f>& imagePoints,
                                         const ProjectionLayout& layout,
                                         cv::Mat& projectMatrix) const
{
    const auto targetPoints = defaultProjectKeypoints(cameraName, layout);
    if (imagePoints.size() != 4 || targetPoints.size() != 4) {
        return false;
    }

    projectMatrix = cv::getPerspectiveTransform(imagePoints, targetPoints);
    return !projectMatrix.empty();
}

} // namespace svs
} // namespace bsp_perf
