#include "SvsProjector.hpp"
#include <cstring>
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

    const auto outputSize = layout.outputSize();
    return cv::Size(static_cast<int>(outputSize.width), static_cast<int>(outputSize.height));
}

bool SvsProjector::project(const cv::Mat& src,
                           const CameraParameters& params,
                           const CameraConfig& cameraConfig,
                           const ProjectionLayout& layout,
                           cv::Mat& dst) const
{
    const cv::Mat projectMatrix = toCvMat(params.projectMatrix);
    if (src.empty() || projectMatrix.empty()) {
        return false;
    }

    cv::Mat undistorted;
    if (!undistortByRemap(src, params, undistorted)) {
        return false;
    }

    cv::warpPerspective(undistorted, dst, projectMatrix, projectSize(cameraConfig.name, layout));
    applyFlip(cameraConfig.flipMode, dst);
    return !dst.empty();
}

cv::Mat SvsProjector::toCvMat(const Matrix& matrix) const
{
    if (matrix.empty() || matrix.values.size() != static_cast<size_t>(matrix.rows) * matrix.cols) {
        return {};
    }

    cv::Mat out(static_cast<int>(matrix.rows), static_cast<int>(matrix.cols), CV_64F);
    std::memcpy(out.data, matrix.values.data(), matrix.values.size() * sizeof(double));
    return out;
}

bool SvsProjector::undistortByRemap(const cv::Mat& src, const CameraParameters& params, cv::Mat& dst) const
{
    cv::Mat cameraMatrix = toCvMat(params.cameraMatrix);
    cv::Mat distCoeffs = toCvMat(params.distCoeffs);
    cv::Mat scaleXY = toCvMat(params.scaleXY);
    cv::Mat shiftXY = toCvMat(params.shiftXY);
    if (src.empty() || cameraMatrix.empty() || distCoeffs.empty() ||
        scaleXY.empty() || shiftXY.empty() || params.size.empty()) {
        return false;
    }

    cv::Mat newCameraMatrix;
    cameraMatrix.convertTo(newCameraMatrix, CV_64F);
    if (newCameraMatrix.empty() || scaleXY.total() < 2 || shiftXY.total() < 2) {
        return false;
    }

    newCameraMatrix.at<double>(0, 0) *= scaleXY.at<double>(0);
    newCameraMatrix.at<double>(1, 1) *= scaleXY.at<double>(1);
    newCameraMatrix.at<double>(0, 2) += shiftXY.at<double>(0);
    newCameraMatrix.at<double>(1, 2) += shiftXY.at<double>(1);

    cv::Mat map1;
    cv::Mat map2;
    cv::fisheye::initUndistortRectifyMap(
        cameraMatrix,
        distCoeffs,
        cv::Mat(),
        newCameraMatrix,
        cv::Size(static_cast<int>(params.size.width), static_cast<int>(params.size.height)),
        CV_16SC2,
        map1,
        map2);
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
