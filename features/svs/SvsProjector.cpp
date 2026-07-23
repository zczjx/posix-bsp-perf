#include "SvsProjector.hpp"

#include <cmath>
#include <opencv2/imgproc.hpp>

namespace bsp_perf
{
namespace svs
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinDepth = 1e-9;

double toRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

cv::Matx33d rotationMatrix(const CameraPose& pose)
{
    const double roll = toRadians(pose.roll);
    const double pitch = toRadians(pose.pitch);
    const double yaw = toRadians(pose.yaw);
    const double cr = std::cos(roll);
    const double sr = std::sin(roll);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);

    return {
        cp * cy, cy * sp * sr - sy * cr, -cy * sp * cr - sy * sr,
        cp * sy, sy * sp * sr + cy * cr, -sy * sp * cr + cy * sr,
        sp, -cp * sr, cp * cr,
    };
}
} // namespace

bool SvsProjector::setup(const SurroundViewConfig& config)
{
    if (config.outputWidth == 0 || config.outputHeight == 0 ||
        config.groundExtent.front <= 0.0 || config.groundExtent.rear <= 0.0 ||
        config.groundExtent.left <= 0.0 || config.groundExtent.right <= 0.0 ||
        config.projectionRegion.nearDistance <= 0.0 ||
        config.projectionRegion.farDistance <= config.projectionRegion.nearDistance ||
        config.projectionRegion.nearHalfWidth <= 0.0 ||
        config.projectionRegion.farHalfWidth <= 0.0 ||
        config.featherPower <= 0.0) {
        return false;
    }

    for (const auto& camera : config.cameras) {
        if (camera.name.empty() || camera.fov <= 0.0 || camera.fov >= 180.0) {
            return false;
        }
    }

    m_config = config;
    for (auto& cache : m_cache) {
        cache = WarpCache{};
    }
    m_ready = true;
    return true;
}

bool SvsProjector::project(size_t cameraIndex,
                           const cv::Mat& source,
                           cv::Mat& warped,
                           cv::Mat& weight)
{
    if (!m_ready || cameraIndex >= kCameraCount || source.empty()) {
        return false;
    }

    auto& cache = m_cache[cameraIndex];
    if (cache.homography.empty() || cache.sourceWidth != source.cols ||
        cache.sourceHeight != source.rows) {
        if (!buildWarp(cameraIndex, source.cols, source.rows, cache)) {
            return false;
        }
    }

    cv::warpPerspective(
        source,
        warped,
        cache.homography,
        cv::Size(static_cast<int>(m_config.outputWidth),
                 static_cast<int>(m_config.outputHeight)),
        cv::INTER_LINEAR,
        cv::BORDER_CONSTANT,
        cv::Scalar::all(0));
    weight = cache.weight;
    return !warped.empty();
}

void SvsProjector::reset()
{
    for (auto& cache : m_cache) {
        cache = WarpCache{};
    }
    m_ready = false;
}

bool SvsProjector::buildWarp(size_t cameraIndex,
                             int sourceWidth,
                             int sourceHeight,
                             WarpCache& cache) const
{
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        return false;
    }

    const auto& camera = m_config.cameras[cameraIndex];
    const auto& pose = camera.pose;
    const double yaw = toRadians(pose.yaw);
    const cv::Vec2d forward(std::cos(yaw), std::sin(yaw));
    const cv::Vec2d right(-std::sin(yaw), std::cos(yaw));
    const cv::Vec2d origin(pose.x, pose.y);

    std::array<cv::Vec3d, 4> vehiclePoints;
    size_t pointIndex = 0;
    for (const auto& region : {
             std::pair<double, double>{m_config.projectionRegion.nearDistance,
                                       m_config.projectionRegion.nearHalfWidth},
             std::pair<double, double>{m_config.projectionRegion.farDistance,
                                       m_config.projectionRegion.farHalfWidth}}) {
        for (const double lateral : {-region.second, region.second}) {
            const cv::Vec2d point = origin + forward * region.first + right * lateral;
            vehiclePoints[pointIndex++] = {point[0], point[1], m_config.groundZ};
        }
    }

    const std::array<size_t, 4> pointOrder = {0, 1, 3, 2};
    std::array<cv::Point2f, 4> imagePoints;
    std::array<cv::Point2f, 4> bevPoints;
    const cv::Matx33d vehicleFromCamera = rotationMatrix(pose);
    const cv::Matx33d cameraFromVehicle = vehicleFromCamera.t();
    const cv::Vec3d translation(pose.x, pose.y, pose.z);
    const double focal = static_cast<double>(sourceWidth) /
                         (2.0 * std::tan(toRadians(camera.fov) / 2.0));

    for (size_t i = 0; i < pointOrder.size(); ++i) {
        const cv::Vec3d& vehiclePoint = vehiclePoints[pointOrder[i]];
        const cv::Vec3d local = cameraFromVehicle * (vehiclePoint - translation);
        if (!std::isfinite(local[0]) || std::abs(local[0]) <= kMinDepth) {
            return false;
        }

        const double imageX = focal * local[1] / local[0] +
                              static_cast<double>(sourceWidth) / 2.0;
        const double imageY = focal * -local[2] / local[0] +
                              static_cast<double>(sourceHeight) / 2.0;
        if (!std::isfinite(imageX) || !std::isfinite(imageY)) {
            return false;
        }

        imagePoints[i] = cv::Point2f(static_cast<float>(imageX), static_cast<float>(imageY));
        bevPoints[i] = vehicleToBev(vehiclePoint[0], vehiclePoint[1]);
    }

    cache.homography = cv::getPerspectiveTransform(imagePoints.data(), bevPoints.data());
    if (cache.homography.empty()) {
        return false;
    }

    const cv::Size outputSize(static_cast<int>(m_config.outputWidth),
                              static_cast<int>(m_config.outputHeight));
    const cv::Mat sourceMask(sourceHeight, sourceWidth, CV_8UC1, cv::Scalar(255));
    cv::Mat warpedMask;
    cv::warpPerspective(
        sourceMask,
        warpedMask,
        cache.homography,
        outputSize,
        cv::INTER_NEAREST,
        cv::BORDER_CONSTANT,
        cv::Scalar(0));
    cv::threshold(warpedMask, warpedMask, 0, 1, cv::THRESH_BINARY);

    cv::Mat distance;
    cv::distanceTransform(warpedMask, distance, cv::DIST_L2, 3);
    double maxDistance = 0.0;
    cv::minMaxLoc(distance, nullptr, &maxDistance);
    if (maxDistance > 0.0) {
        distance /= maxDistance;
    }
    cv::pow(distance, m_config.featherPower, distance);
    cache.weight = 0.05F + 0.95F * distance;

    cv::Mat maskFloat;
    warpedMask.convertTo(maskFloat, CV_32FC1);
    cache.weight = cache.weight.mul(maskFloat);
    cache.sourceWidth = sourceWidth;
    cache.sourceHeight = sourceHeight;
    return true;
}

cv::Point2f SvsProjector::vehicleToBev(double x, double y) const
{
    const double bevX = (y + m_config.groundExtent.left) /
                        (m_config.groundExtent.left + m_config.groundExtent.right) *
                        static_cast<double>(m_config.outputWidth - 1);
    const double bevY = (m_config.groundExtent.front - x) /
                        (m_config.groundExtent.front + m_config.groundExtent.rear) *
                        static_cast<double>(m_config.outputHeight - 1);
    return {static_cast<float>(bevX), static_cast<float>(bevY)};
}

} // namespace svs
} // namespace bsp_perf
