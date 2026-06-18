#ifndef __BSP_IMAGE_OPENCV_IMAGE_ADAPTER_HPP__
#define __BSP_IMAGE_OPENCV_IMAGE_ADAPTER_HPP__

#include "ImageBuffer.hpp"
#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

namespace bsp_perf
{
namespace bsp_image
{

class OpenCvImageAdapter
{
public:
    static int cvTypeForFormat(const std::string& format)
    {
        if (format == "BGR888" || format == "RGB888") {
            return CV_8UC3;
        }
        if (format == "BGRA8888" || format == "RGBA8888") {
            return CV_8UC4;
        }
        if (format == "GRAY8") {
            return CV_8UC1;
        }
        return -1;
    }

    static bool toMat(const ImageView& view, cv::Mat& mat)
    {
        const int cvType = cvTypeForFormat(view.desc.format);
        if (view.empty() || cvType < 0 || view.memoryType != ImageMemoryType::Host) {
            return false;
        }

        const size_t step = view.planes[0].rowStride > 0 ? view.planes[0].rowStride : defaultRowStride(view.desc, cvType);
        mat = cv::Mat(static_cast<int>(view.desc.height),
                      static_cast<int>(view.desc.width),
                      cvType,
                      view.planes[0].data,
                      step);
        return !mat.empty();
    }

    static bool toRgbMat(const ImageView& view, cv::Mat& rgb)
    {
        cv::Mat mat;
        if (!toMat(view, mat)) {
            return false;
        }

        if (view.desc.format == "RGB888") {
            rgb = mat;
        } else if (view.desc.format == "RGBA8888") {
            cv::cvtColor(mat, rgb, cv::COLOR_RGBA2RGB);
        } else if (view.desc.format == "BGRA8888") {
            cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGB);
        } else if (view.desc.format == "BGR888") {
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        } else {
            return false;
        }
        return !rgb.empty();
    }

    static bool fromMat(const cv::Mat& mat, const std::string& format, ImageBuffer& buffer)
    {
        if (mat.empty()) {
            return false;
        }

        auto data = std::make_shared<std::vector<uint8_t>>(mat.total() * mat.elemSize());
        if (mat.isContinuous()) {
            std::memcpy(data->data(), mat.data, data->size());
        } else {
            const size_t rowBytes = static_cast<size_t>(mat.cols) * mat.elemSize();
            for (int row = 0; row < mat.rows; ++row) {
                std::memcpy(data->data() + row * rowBytes, mat.ptr(row), rowBytes);
            }
        }

        ImageDesc desc{};
        desc.width = static_cast<uint32_t>(mat.cols);
        desc.height = static_cast<uint32_t>(mat.rows);
        desc.widthStride = static_cast<uint32_t>(mat.cols);
        desc.heightStride = static_cast<uint32_t>(mat.rows);
        desc.format = format;
        desc.dataSize = data->size();
        buffer.view = makeHostImageView(data->data(), desc, static_cast<uint32_t>(mat.cols * mat.elemSize()));
        buffer.owner = data;
        return true;
    }

private:
    static size_t defaultRowStride(const ImageDesc& desc, int cvType)
    {
        const size_t channels = static_cast<size_t>(CV_MAT_CN(cvType));
        return desc.widthStride > 0 ? desc.widthStride * channels : desc.width * channels;
    }
};

} // namespace bsp_image
} // namespace bsp_perf

#endif // __BSP_IMAGE_OPENCV_IMAGE_ADAPTER_HPP__
