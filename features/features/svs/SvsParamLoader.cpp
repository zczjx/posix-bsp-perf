#include "SvsParamLoader.hpp"
#include <opencv2/core/persistence.hpp>

namespace bsp_perf
{
namespace svs
{

bool SvsParamLoader::loadCameraParameters(const std::string& path, CameraParameters& params) const
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }

    params.cameraMatrix = fs["camera_matrix"].mat();
    params.distCoeffs = fs["dist_coeffs"].mat();
    params.projectMatrix = fs["project_matrix"].mat();
    params.shiftXY = fs["shift_xy"].mat();
    params.scaleXY = fs["scale_xy"].mat();

    cv::Mat resolution = fs["resolution"].mat();
    if (!resolution.empty() && resolution.total() >= 2) {
        params.size = cv::Size(resolution.at<int>(0), resolution.at<int>(1));
    }

    return !params.cameraMatrix.empty() && !params.distCoeffs.empty() && !params.size.empty();
}

bool SvsParamLoader::saveProjectMatrix(const std::string& path, const CameraParameters& params) const
{
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        return false;
    }

    if (!params.projectMatrix.empty()) {
        fs << "project_matrix" << params.projectMatrix;
    }

    return true;
}

} // namespace svs
} // namespace bsp_perf
