#include "SvsBlender.hpp"
#include <algorithm>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace bsp_perf
{
namespace svs
{

namespace
{
struct BgrStats
{
    int b{0};
    int g{0};
    int r{0};
};

uint8_t clipToByte(float value)
{
    return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, value)));
}

bool calcBgrStats(const cv::Mat& image, BgrStats& stats)
{
    if (image.empty() || image.channels() != 3) {
        return false;
    }

    const int pixelCount = image.rows * image.cols;
    if (pixelCount == 0) {
        return false;
    }

    int64_t b = 0;
    int64_t g = 0;
    int64_t r = 0;
    for (int row = 0; row < image.rows; ++row) {
        const auto* pixel = image.ptr<uint8_t>(row);
        for (int col = 0; col < image.cols; ++col) {
            b += pixel[0];
            g += pixel[1];
            r += pixel[2];
            pixel += 3;
        }
    }

    stats.b = static_cast<int>(b / pixelCount);
    stats.g = static_cast<int>(g / pixelCount);
    stats.r = static_cast<int>(r / pixelCount);
    return true;
}

void applyBgrGain(cv::Mat& image, float rGain, float gGain, float bGain)
{
    for (int row = 0; row < image.rows; ++row) {
        auto* pixel = image.ptr<uint8_t>(row);
        for (int col = 0; col < image.cols; ++col) {
            pixel[0] = clipToByte(static_cast<float>(pixel[0]) * bGain);
            pixel[1] = clipToByte(static_cast<float>(pixel[1]) * gGain);
            pixel[2] = clipToByte(static_cast<float>(pixel[2]) * rGain);
            pixel += 3;
        }
    }
}

std::string joinPath(const std::string& root, const std::string& path)
{
    if (path.empty() || root.empty() || path.front() == '/') {
        return path;
    }
    if (root.back() == '/') {
        return root + path;
    }
    return root + "/" + path;
}
} // namespace

bool SvsBlender::setup(const SurroundViewConfig& config)
{
    reset();
    m_layout = config.layout;

    if (!loadWeights(joinPath(config.dataRoot, config.weightImagePath))) {
        return false;
    }

    if (!config.carImagePath.empty()) {
        m_hasVehicleImage = loadVehicleImage(joinPath(config.dataRoot, config.carImagePath));
    }

    m_ready = true;
    return true;
}

void SvsBlender::applyAwbAndLuminanceBalance(std::array<cv::Mat, kCameraCount>& images) const
{
    std::array<BgrStats, kCameraCount> stats;
    std::array<int, kCameraCount> gray{};
    float grayAverage = 0.0f;

    for (size_t i = 0; i < kCameraCount; ++i) {
        if (!calcBgrStats(images[i], stats[i])) {
            return;
        }
        gray[i] = stats[i].r * 20 + stats[i].g * 60 + stats[i].b;
        if (gray[i] == 0 || stats[i].r == 0 || stats[i].g == 0 || stats[i].b == 0) {
            return;
        }
        grayAverage += static_cast<float>(gray[i]);
    }

    grayAverage /= static_cast<float>(kCameraCount);
    for (size_t i = 0; i < kCameraCount; ++i) {
        const float lumGain = grayAverage / static_cast<float>(gray[i]);
        const float rGain = static_cast<float>(stats[i].g) * lumGain / static_cast<float>(stats[i].r);
        const float gGain = lumGain;
        const float bGain = static_cast<float>(stats[i].g) * lumGain / static_cast<float>(stats[i].b);
        applyBgrGain(images[i], rGain, gGain, bGain);
    }
}

bool SvsBlender::blend(const std::array<cv::Mat, kCameraCount>& projectedImages, cv::Mat& output) const
{
    if (!m_ready) {
        return false;
    }

    const int totalW = m_layout.totalWidth();
    const int totalH = m_layout.totalHeight();
    const int xl = m_layout.vehicleLeft();
    const int xr = m_layout.vehicleRight();
    const int yt = m_layout.vehicleTop();
    const int yb = m_layout.vehicleBottom();

    output = cv::Mat(cv::Size(totalW, totalH), CV_8UC3, cv::Scalar(0, 0, 0));

    if (m_hasVehicleImage) {
        m_vehicleImage.copyTo(output(cv::Rect(xl, yt, m_vehicleImage.cols, m_vehicleImage.rows)));
    }

    projectedImages[0](cv::Rect(xl, 0, xr - xl, yt)).copyTo(output(cv::Rect(xl, 0, xr - xl, yt)));
    projectedImages[1](cv::Rect(0, yt, xl, yb - yt)).copyTo(output(cv::Rect(0, yt, xl, yb - yt)));
    projectedImages[3](cv::Rect(0, yt, xl, yb - yt)).copyTo(output(cv::Rect(xr, yt, totalW - xr, yb - yt)));
    projectedImages[2](cv::Rect(xl, 0, xr - xl, yt)).copyTo(output(cv::Rect(xl, yb, xr - xl, yt)));

    cv::Rect roi(0, 0, xl, yt);
    mergeImage(projectedImages[0](roi), projectedImages[1](roi), m_weights[2], output(roi));

    roi = cv::Rect(xr, 0, xl, yt);
    mergeImage(projectedImages[0](roi), projectedImages[3](cv::Rect(0, 0, xl, yt)), m_weights[1],
               output(cv::Rect(xr, 0, xl, yt)));

    roi = cv::Rect(0, yb, xl, yt);
    mergeImage(projectedImages[2](cv::Rect(0, 0, xl, yt)), projectedImages[1](roi), m_weights[0], output(roi));

    roi = cv::Rect(xr, 0, xl, yt);
    mergeImage(projectedImages[2](roi), projectedImages[3](cv::Rect(0, yb, xl, yt)), m_weights[3],
               output(cv::Rect(xr, yb, xl, yt)));

    return true;
}

void SvsBlender::reset()
{
    m_vehicleImage.release();
    for (auto& weight : m_weights) {
        weight.release();
    }
    m_hasVehicleImage = false;
    m_ready = false;
}

bool SvsBlender::loadWeights(const std::string& path)
{
    cv::Mat weights = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (weights.empty() || weights.channels() != static_cast<int>(kCameraCount)) {
        return false;
    }

    for (auto& weight : m_weights) {
        weight = cv::Mat(weights.size(), CV_32FC1, cv::Scalar(0.0f));
    }

    for (int row = 0; row < weights.rows; ++row) {
        const auto* src = weights.ptr<uint8_t>(row);
        auto* w0 = m_weights[0].ptr<float>(row);
        auto* w1 = m_weights[1].ptr<float>(row);
        auto* w2 = m_weights[2].ptr<float>(row);
        auto* w3 = m_weights[3].ptr<float>(row);
        for (int col = 0; col < weights.cols; ++col) {
            w0[col] = static_cast<float>(src[0]) / 255.0f;
            w1[col] = static_cast<float>(src[1]) / 255.0f;
            w2[col] = static_cast<float>(src[2]) / 255.0f;
            w3[col] = static_cast<float>(src[3]) / 255.0f;
            src += 4;
        }
    }

    return true;
}

bool SvsBlender::loadVehicleImage(const std::string& path)
{
    cv::Mat vehicle = cv::imread(path, cv::IMREAD_COLOR);
    if (vehicle.empty()) {
        return false;
    }

    cv::resize(vehicle, m_vehicleImage,
               cv::Size(m_layout.vehicleRight() - m_layout.vehicleLeft(),
                        m_layout.vehicleBottom() - m_layout.vehicleTop()));
    return !m_vehicleImage.empty();
}

void SvsBlender::mergeImage(const cv::Mat& src1, const cv::Mat& src2, const cv::Mat& weight, cv::Mat out) const
{
    if (src1.empty() || src2.empty() || weight.empty() || out.empty() ||
        src1.size() != src2.size() || src1.size() != out.size() ||
        weight.rows < src1.rows || weight.cols < src1.cols) {
        return;
    }

    for (int row = 0; row < src1.rows; ++row) {
        const auto* p1 = src1.ptr<uint8_t>(row);
        const auto* p2 = src2.ptr<uint8_t>(row);
        const auto* w = weight.ptr<float>(row);
        auto* dst = out.ptr<uint8_t>(row);
        for (int col = 0; col < src1.cols; ++col) {
            dst[0] = clipToByte(static_cast<float>(p1[0]) * w[col] + static_cast<float>(p2[0]) * (1.0f - w[col]));
            dst[1] = clipToByte(static_cast<float>(p1[1]) * w[col] + static_cast<float>(p2[1]) * (1.0f - w[col]));
            dst[2] = clipToByte(static_cast<float>(p1[2]) * w[col] + static_cast<float>(p2[2]) * (1.0f - w[col]));
            p1 += 3;
            p2 += 3;
            dst += 3;
        }
    }
}

} // namespace svs
} // namespace bsp_perf
