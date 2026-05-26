#include "SvsProjector.hpp"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace bsp_perf
{
namespace svs
{

cv::Size SvsProjector::projectSize(const std::string& cameraName, const ProjectionLayout& layout) const
{
    if (cameraName == "front" || cameraName == "back") {
        return cv::Size(layout.totalWidth(), layout.vehicleTop());
    }
    if (cameraName == "left" || cameraName == "right") {
        return cv::Size(layout.totalHeight(), layout.vehicleLeft());
    }

    return layout.outputSize();
}

bool SvsProjector::project(const cv::Mat& src,
                           const CameraParameters& params,
                           const CameraConfig& cameraConfig,
                           const ProjectionLayout& layout,
                           cv::Mat& dst) const
{
    if (src.empty() || params.projectMatrix.empty()) {
        return false;
    }

    cv::Mat undistorted;
    if (!undistortByRemap(src, params, undistorted)) {
        return false;
    }

    cv::warpPerspective(undistorted, dst, params.projectMatrix, projectSize(cameraConfig.name, layout));
    applyFlip(cameraConfig.flipMode, dst);
    return !dst.empty();
}

bool SvsProjector::undistortByRemap(const cv::Mat& src, const CameraParameters& params, cv::Mat& dst) const
{
    if (src.empty() || params.cameraMatrix.empty() || params.distCoeffs.empty() ||
        params.scaleXY.empty() || params.shiftXY.empty() || params.size.empty()) {
        return false;
    }

    cv::Mat newCameraMatrix;
    params.cameraMatrix.convertTo(newCameraMatrix, CV_64F);
    const auto* scale = reinterpret_cast<const float*>(params.scaleXY.data);
    const auto* shift = reinterpret_cast<const float*>(params.shiftXY.data);

    if (newCameraMatrix.empty() || scale == nullptr || shift == nullptr) {
        return false;
    }

    newCameraMatrix.at<double>(0, 0) *= static_cast<double>(scale[0]);
    newCameraMatrix.at<double>(1, 1) *= static_cast<double>(scale[1]);
    newCameraMatrix.at<double>(0, 2) += static_cast<double>(shift[0]);
    newCameraMatrix.at<double>(1, 2) += static_cast<double>(shift[1]);

    cv::Mat map1;
    cv::Mat map2;
    cv::fisheye::initUndistortRectifyMap(
        params.cameraMatrix, params.distCoeffs, cv::Mat(), newCameraMatrix, params.size, CV_16SC2, map1, map2);
    cv::remap(src, dst, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);

    return !dst.empty();
}

void SvsProjector::applyFlip(const std::string& flipMode, cv::Mat& image) const
{
    if (flipMode == "r+") {
        cv::rotate(image, image, cv::ROTATE_90_CLOCKWISE);
    } else if (flipMode == "r-") {
        cv::rotate(image, image, cv::ROTATE_90_COUNTERCLOCKWISE);
    } else if (flipMode == "m") {
        cv::rotate(image, image, cv::ROTATE_180);
    }
}

} // namespace svs
} // namespace bsp_perf
