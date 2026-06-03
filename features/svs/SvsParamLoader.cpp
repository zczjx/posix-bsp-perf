#include "SvsParamLoader.hpp"
#include <cstring>
#include <opencv2/core/persistence.hpp>

namespace bsp_perf
{
namespace svs
{
namespace
{
Matrix toMatrix(const cv::Mat& mat)
{
    Matrix matrix;
    if (mat.empty()) {
        return matrix;
    }

    cv::Mat mat64;
    mat.convertTo(mat64, CV_64F);
    matrix.rows = static_cast<uint32_t>(mat64.rows);
    matrix.cols = static_cast<uint32_t>(mat64.cols);
    matrix.values.resize(mat64.total());
    std::memcpy(matrix.values.data(), mat64.ptr<double>(), matrix.values.size() * sizeof(double));
    return matrix;
}

cv::Mat toCvMat(const Matrix& matrix)
{
    if (matrix.empty() || matrix.values.size() != static_cast<size_t>(matrix.rows) * matrix.cols) {
        return {};
    }

    cv::Mat mat(static_cast<int>(matrix.rows), static_cast<int>(matrix.cols), CV_64F);
    std::memcpy(mat.data, matrix.values.data(), matrix.values.size() * sizeof(double));
    return mat;
}
} // namespace

bool SvsParamLoader::loadCameraParameters(const std::string& path, CameraParameters& params) const
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }

    params.cameraMatrix = toMatrix(fs["camera_matrix"].mat());
    params.distCoeffs = toMatrix(fs["dist_coeffs"].mat());
    params.projectMatrix = toMatrix(fs["project_matrix"].mat());
    params.shiftXY = toMatrix(fs["shift_xy"].mat());
    params.scaleXY = toMatrix(fs["scale_xy"].mat());

    cv::Mat resolution = fs["resolution"].mat();
    if (!resolution.empty() && resolution.total() >= 2) {
        params.size = {
            static_cast<uint32_t>(resolution.at<int>(0)),
            static_cast<uint32_t>(resolution.at<int>(1)),
        };
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
        fs << "project_matrix" << toCvMat(params.projectMatrix);
    }

    return true;
}

} // namespace svs
} // namespace bsp_perf
