#include "OpenCVUtils.hpp"

#include <bsp_image/OpenCvImageAdapter.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace g2d_image_ops {
namespace {

using ImageBuffer = bsp_perf::bsp_image::ImageBuffer;

int alignUp(int value, int alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

cv::Mat matFromBuffer(const std::shared_ptr<ImageBuffer>& buffer)
{
    cv::Mat mat;
    if (!buffer || !bsp_perf::bsp_image::OpenCvImageAdapter::toMat(buffer->view, mat)) {
        return {};
    }
    return mat.clone();
}

double computePsnr(const cv::Mat& a, const cv::Mat& b)
{
    if (a.empty() || b.empty() || a.size() != b.size() || a.type() != b.type()) {
        return 0.0;
    }
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);
    const cv::Scalar sum = cv::sum(diff);
    const double sse = sum[0] + sum[1] + sum[2] + sum[3];
    if (sse <= 1e-10) {
        return 100.0;
    }
    const double mse = sse / static_cast<double>(a.total() * a.channels());
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

bool saveRgbaBuffer(const std::filesystem::path& path, const std::shared_ptr<ImageBuffer>& buffer)
{
    const cv::Mat rgba = matFromBuffer(buffer);
    if (rgba.empty()) {
        return false;
    }
    cv::Mat bgra;
    cv::cvtColor(rgba, bgra, cv::COLOR_RGBA2BGRA);
    return cv::imwrite(path.string(), bgra);
}

cv::Mat drawRectangle(
    const cv::Mat& src,
    int x,
    int y,
    int width,
    int height,
    const cv::Scalar& color,
    int thickness)
{
    cv::Mat ref = src.clone();
    cv::rectangle(ref, cv::Rect(x, y, width, height), color, thickness);
    return ref;
}

} // namespace

cv::Mat loadRgbaInputOpenCV(const std::string& path)
{
    cv::Mat input = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (input.empty()) {
        return {};
    }

    cv::Mat rgba;
    if (input.channels() == 4) {
        cv::cvtColor(input, rgba, cv::COLOR_BGRA2RGBA);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, rgba, cv::COLOR_BGR2RGBA);
    } else if (input.channels() == 1) {
        cv::cvtColor(input, rgba, cv::COLOR_GRAY2RGBA);
    } else {
        return {};
    }

    const int targetWidth = std::max(128, alignUp(rgba.cols, 16));
    const int targetHeight = std::max(128, alignUp(rgba.rows, 2));
    if (targetWidth != rgba.cols || targetHeight != rgba.rows) {
        cv::copyMakeBorder(
            rgba,
            rgba,
            0,
            targetHeight - rgba.rows,
            0,
            targetWidth - rgba.cols,
            cv::BORDER_CONSTANT,
            cv::Scalar(0, 0, 0, 255));
    }
    return rgba;
}

bool saveRgbaPngOpenCV(const std::filesystem::path& path, const cv::Mat& rgba)
{
    if (rgba.empty()) {
        return false;
    }
    cv::Mat bgra;
    cv::cvtColor(rgba, bgra, cv::COLOR_RGBA2BGRA);
    return cv::imwrite(path.string(), bgra);
}

OpResult finishOpOpenCV(
    const std::string& name,
    int ret,
    const std::shared_ptr<ImageBuffer>& output,
    const cv::Mat& reference,
    const std::filesystem::path& outputDir,
    double threshold)
{
    OpResult result{name, "FAIL", 0.0, ""};
    if (ret != 0) {
        result.note = "g2d returned failure";
        return result;
    }

    const auto imagePath = outputDir / (name + ".png");
    saveRgbaBuffer(imagePath, output);
    const cv::Mat actual = matFromBuffer(output);
    result.psnr = computePsnr(actual, reference);
    result.status = result.psnr >= threshold ? "PASS" : "FAIL";
    return result;
}

OpResult finishCvtColorBgraOpenCV(
    int ret,
    const std::shared_ptr<ImageBuffer>& output,
    const cv::Mat& rgba,
    const std::filesystem::path& outputDir)
{
    cv::Mat ref;
    cv::cvtColor(rgba, ref, cv::COLOR_RGBA2BGRA);
    cv::Mat actual;
    bsp_perf::bsp_image::OpenCvImageAdapter::toMat(output->view, actual);
    cv::imwrite((outputDir / "cvtcolor_bgra.png").string(), actual);
    OpResult result{"cvtcolor_bgra", ret == 0 ? "PASS" : "FAIL", ret == 0 ? computePsnr(actual, ref) : 0.0, ""};
    // RGA is bit exact (100 dB); VIC rounds by +-1 LSB (~59.8 dB). 50 dB still
    // catches a swapped channel order or a wrong format.
    if (result.psnr < 50.0) {
        result.status = "FAIL";
    }
    return result;
}

cv::Mat resizeReferenceOpenCV(const cv::Mat& src, int width, int height)
{
    cv::Mat ref;
    cv::resize(src, ref, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
    return ref;
}

cv::Mat cropReferenceOpenCV(const cv::Mat& src, int x, int y, int width, int height)
{
    return src(cv::Rect(x, y, width, height)).clone();
}

cv::Mat resizeOntoCanvasOpenCV(
    const cv::Mat& src,
    int canvasWidth,
    int canvasHeight,
    int x,
    int y,
    int dstWidth,
    int dstHeight)
{
    cv::Mat ref(canvasHeight, canvasWidth, CV_8UC4, cv::Scalar(0, 0, 0, 255));
    cv::Mat resized = resizeReferenceOpenCV(src, dstWidth, dstHeight);
    resized.copyTo(ref(cv::Rect(x, y, dstWidth, dstHeight)));
    return ref;
}

cv::Mat filledRectangleReferenceOpenCV(
    const cv::Mat& src,
    int x,
    int y,
    int width,
    int height,
    const cv::Scalar& color)
{
    return drawRectangle(src, x, y, width, height, color, cv::FILLED);
}

cv::Mat strokedRectangleReferenceOpenCV(
    const cv::Mat& src,
    int x,
    int y,
    int width,
    int height,
    const cv::Scalar& color,
    int thickness)
{
    return drawRectangle(src, x, y, width, height, color, thickness);
}

cv::Mat makeRedBlendOverlayOpenCV(const cv::Mat& src)
{
    cv::Mat overlay(src.size(), CV_8UC4, cv::Scalar(0, 0, 0, 0));
    cv::rectangle(
        overlay,
        cv::Rect(src.cols / 4, src.rows / 4, src.cols / 2, src.rows / 2),
        cv::Scalar(255, 0, 0, 128),
        cv::FILLED);
    return overlay;
}

cv::Mat srcOverReferenceOpenCV(const cv::Mat& background, const cv::Mat& overlay)
{
    cv::Mat ref = background.clone();
    for (int y = 0; y < ref.rows; ++y) {
        for (int x = 0; x < ref.cols; ++x) {
            const cv::Vec4b fg = overlay.at<cv::Vec4b>(y, x);
            if (fg[3] == 0) {
                continue;
            }
            cv::Vec4b& bg = ref.at<cv::Vec4b>(y, x);
            const float a = fg[3] / 255.0F;
            for (int c = 0; c < 3; ++c) {
                bg[c] = static_cast<uint8_t>(fg[c] * a + bg[c] * (1.0F - a));
            }
            bg[3] = 255;
        }
    }
    return ref;
}

cv::Mat rotateReferenceOpenCV(const cv::Mat& src, int clockwiseDegrees)
{
    int rotateCode = cv::ROTATE_90_CLOCKWISE;
    if (clockwiseDegrees == 180) {
        rotateCode = cv::ROTATE_180;
    } else if (clockwiseDegrees == 270) {
        rotateCode = cv::ROTATE_90_COUNTERCLOCKWISE;
    }
    cv::Mat ref;
    cv::rotate(src, ref, rotateCode);
    return ref;
}

cv::Mat flipReferenceOpenCV(const cv::Mat& src, OpenCVAxisFlip flip)
{
    int flipCode = flip == OpenCVAxisFlip::Horizontal ? 1 : 0;
    if (flip == OpenCVAxisFlip::Both) {
        flipCode = -1;
    }
    cv::Mat ref;
    cv::flip(src, ref, flipCode);
    return ref;
}

} // namespace g2d_image_ops
