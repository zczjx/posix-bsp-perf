#include "SvsBlender.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

namespace bsp_perf
{
namespace svs
{

bool SvsBlender::setup(const SurroundViewConfig& config)
{
    reset();
    if (config.outputWidth == 0 || config.outputHeight == 0 ||
        config.vehicleSize.length <= 0.0 || config.vehicleSize.width <= 0.0 ||
        config.inpaintRadius <= 0.0) {
        return false;
    }

    m_config = config;
    if (!config.vehicleImagePath.empty()) {
        cv::Mat image = cv::imread(config.vehicleImagePath, cv::IMREAD_UNCHANGED);
        if (!image.empty() && image.channels() == 4) {
            cv::cvtColor(image, m_vehicleImage, cv::COLOR_BGRA2RGBA);
        } else if (!image.empty() && image.channels() == 3) {
            cv::cvtColor(image, m_vehicleImage, cv::COLOR_BGR2RGB);
        }
    }

    m_ready = true;
    return true;
}

bool SvsBlender::blend(const std::array<cv::Mat, kCameraCount>& warpedImages,
                       const std::array<cv::Mat, kCameraCount>& weights,
                       cv::Mat& output) const
{
    if (!m_ready) {
        return false;
    }

    const int width = static_cast<int>(m_config.outputWidth);
    const int height = static_cast<int>(m_config.outputHeight);
    cv::Mat accumulator(height, width, CV_32FC3, cv::Scalar::all(0));
    cv::Mat weightSum(height, width, CV_32FC1, cv::Scalar(0));

    for (size_t cameraIndex = 0; cameraIndex < kCameraCount; ++cameraIndex) {
        const auto& image = warpedImages[cameraIndex];
        const auto& weight = weights[cameraIndex];
        if (image.empty() || image.type() != CV_8UC3 || image.size() != accumulator.size() ||
            weight.empty() || weight.type() != CV_32FC1 || weight.size() != weightSum.size()) {
            return false;
        }

        for (int row = 0; row < height; ++row) {
            const auto* source = image.ptr<cv::Vec3b>(row);
            const auto* sourceWeight = weight.ptr<float>(row);
            auto* accumulated = accumulator.ptr<cv::Vec3f>(row);
            auto* accumulatedWeight = weightSum.ptr<float>(row);
            for (int col = 0; col < width; ++col) {
                const float value = sourceWeight[col];
                accumulated[col][0] += static_cast<float>(source[col][0]) * value;
                accumulated[col][1] += static_cast<float>(source[col][1]) * value;
                accumulated[col][2] += static_cast<float>(source[col][2]) * value;
                accumulatedWeight[col] += value;
            }
        }
    }

    output = cv::Mat(height, width, CV_8UC3, cv::Scalar::all(0));
    cv::Mat validMask;
    cv::compare(weightSum, 1e-5, validMask, cv::CMP_GT);
    for (int row = 0; row < height; ++row) {
        const auto* accumulated = accumulator.ptr<cv::Vec3f>(row);
        const auto* accumulatedWeight = weightSum.ptr<float>(row);
        const auto* valid = validMask.ptr<uint8_t>(row);
        auto* destination = output.ptr<cv::Vec3b>(row);
        for (int col = 0; col < width; ++col) {
            if (valid[col] == 0) {
                continue;
            }
            const float inverseWeight = 1.0F / accumulatedWeight[col];
            for (int channel = 0; channel < 3; ++channel) {
                destination[col][channel] = cv::saturate_cast<uint8_t>(
                    accumulated[col][channel] * inverseWeight);
            }
        }
    }

    if (m_config.fillUncovered && cv::countNonZero(validMask) < width * height) {
        cv::Mat invalidMask;
        cv::Mat inpainted;
        cv::bitwise_not(validMask, invalidMask);
        cv::inpaint(output, invalidMask, inpainted, m_config.inpaintRadius, cv::INPAINT_TELEA);
        output = std::move(inpainted);
    }

    drawVehicle(output);
    return true;
}

void SvsBlender::reset()
{
    m_vehicleImage.release();
    m_ready = false;
}

cv::Point SvsBlender::vehicleToBev(double x, double y) const
{
    const double bevX = (y + m_config.groundExtent.left) /
                        (m_config.groundExtent.left + m_config.groundExtent.right) *
                        static_cast<double>(m_config.outputWidth - 1);
    const double bevY = (m_config.groundExtent.front - x) /
                        (m_config.groundExtent.front + m_config.groundExtent.rear) *
                        static_cast<double>(m_config.outputHeight - 1);
    return {
        static_cast<int>(std::lround(bevX)),
        static_cast<int>(std::lround(bevY)),
    };
}

void SvsBlender::drawVehicle(cv::Mat& image) const
{
    std::array<cv::Point, 4> corners = {
        vehicleToBev(m_config.vehicleSize.length / 2.0, -m_config.vehicleSize.width / 2.0),
        vehicleToBev(m_config.vehicleSize.length / 2.0, m_config.vehicleSize.width / 2.0),
        vehicleToBev(-m_config.vehicleSize.length / 2.0, m_config.vehicleSize.width / 2.0),
        vehicleToBev(-m_config.vehicleSize.length / 2.0, -m_config.vehicleSize.width / 2.0),
    };

    if (!m_vehicleImage.empty()) {
        const cv::Rect imageBounds(0, 0, image.cols, image.rows);
        const cv::Rect vehicleBounds = cv::boundingRect(corners) & imageBounds;
        if (vehicleBounds.empty()) {
            return;
        }

        cv::Mat overlay;
        cv::resize(m_vehicleImage, overlay, vehicleBounds.size(), 0.0, 0.0, cv::INTER_AREA);
        cv::Mat target = image(vehicleBounds);
        if (overlay.channels() == 4) {
            for (int row = 0; row < overlay.rows; ++row) {
                const auto* source = overlay.ptr<cv::Vec4b>(row);
                auto* destination = target.ptr<cv::Vec3b>(row);
                for (int col = 0; col < overlay.cols; ++col) {
                    const float alpha = static_cast<float>(source[col][3]) / 255.0F;
                    for (int channel = 0; channel < 3; ++channel) {
                        destination[col][channel] = cv::saturate_cast<uint8_t>(
                            static_cast<float>(source[col][channel]) * alpha +
                            static_cast<float>(destination[col][channel]) * (1.0F - alpha));
                    }
                }
            }
        } else {
            overlay.copyTo(target);
        }
        return;
    }

    cv::fillConvexPoly(image, corners.data(), static_cast<int>(corners.size()), cv::Scalar(35, 35, 35));
    const cv::Point* polygon[] = {corners.data()};
    const int pointCount[] = {static_cast<int>(corners.size())};
    cv::polylines(image, polygon, pointCount, 1, true, cv::Scalar(220, 220, 220), 2);

    const std::array<cv::Point, 3> marker = {
        vehicleToBev(m_config.vehicleSize.length * 0.38, 0.0),
        vehicleToBev(0.0, -m_config.vehicleSize.width * 0.28),
        vehicleToBev(0.0, m_config.vehicleSize.width * 0.28),
    };
    cv::fillConvexPoly(image, marker.data(), static_cast<int>(marker.size()), cv::Scalar(80, 180, 255));
}

} // namespace svs
} // namespace bsp_perf
