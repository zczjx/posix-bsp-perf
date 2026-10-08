#pragma once

#include <bsp_image/ImageBuffer.hpp>

#include <opencv2/core.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace g2d_image_ops {

struct OpResult
{
    std::string name;
    std::string status;
    double psnr{0.0};
    std::string note;
};

enum class OpenCVAxisFlip
{
    Horizontal,
    Vertical,
    Both,
};

cv::Mat loadRgbaInputOpenCV(const std::string& path);

bool saveRgbaPngOpenCV(const std::filesystem::path& path, const cv::Mat& rgba);

OpResult finishOpOpenCV(
    const std::string& name,
    int ret,
    const std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>& output,
    const cv::Mat& reference,
    const std::filesystem::path& outputDir,
    double threshold = 28.0);

OpResult finishCvtColorBgraOpenCV(
    int ret,
    const std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>& output,
    const cv::Mat& rgba,
    const std::filesystem::path& outputDir);

cv::Mat resizeReferenceOpenCV(const cv::Mat& src, int width, int height);

cv::Mat cropReferenceOpenCV(const cv::Mat& src, int x, int y, int width, int height);

cv::Mat resizeOntoCanvasOpenCV(
    const cv::Mat& src,
    int canvasWidth,
    int canvasHeight,
    int x,
    int y,
    int dstWidth,
    int dstHeight);

cv::Mat filledRectangleReferenceOpenCV(
    const cv::Mat& src,
    int x,
    int y,
    int width,
    int height,
    const cv::Scalar& color);

cv::Mat strokedRectangleReferenceOpenCV(
    const cv::Mat& src,
    int x,
    int y,
    int width,
    int height,
    const cv::Scalar& color,
    int thickness);

cv::Mat makeRedBlendOverlayOpenCV(const cv::Mat& src);

cv::Mat srcOverReferenceOpenCV(const cv::Mat& background, const cv::Mat& overlay);

cv::Mat rotateReferenceOpenCV(const cv::Mat& src, int clockwiseDegrees);

cv::Mat flipReferenceOpenCV(const cv::Mat& src, OpenCVAxisFlip flip);

} // namespace g2d_image_ops
