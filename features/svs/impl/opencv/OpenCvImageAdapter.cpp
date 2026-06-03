#include "OpenCvImageAdapter.hpp"
#include <cstring>
#include <vector>

namespace bsp_perf
{
namespace svs
{
namespace
{
int cvTypeForFormat(const std::string& format)
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

size_t defaultRowStride(const bsp_perf::image::ImageDesc& desc, int cvType)
{
    const size_t channels = static_cast<size_t>(CV_MAT_CN(cvType));
    return desc.widthStride > 0 ? desc.widthStride * channels : desc.width * channels;
}
} // namespace

bool OpenCvImageAdapter::toMat(const bsp_perf::image::ImageView& view, cv::Mat& mat)
{
    const int cvType = cvTypeForFormat(view.desc.format);
    if (view.empty() || cvType < 0 || view.memoryType != bsp_perf::image::ImageMemoryType::Host) {
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

bool OpenCvImageAdapter::fromMat(const cv::Mat& mat, const std::string& format, bsp_perf::image::ImageView& view)
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

    view = {};
    view.desc.width = static_cast<uint32_t>(mat.cols);
    view.desc.height = static_cast<uint32_t>(mat.rows);
    view.desc.widthStride = static_cast<uint32_t>(mat.cols);
    view.desc.heightStride = static_cast<uint32_t>(mat.rows);
    view.desc.format = format;
    view.desc.dataSize = data->size();
    view.memoryType = bsp_perf::image::ImageMemoryType::Host;
    view.access = bsp_perf::image::ImageAccess::ReadWrite;
    view.planeCount = 1;
    view.planes[0].data = data->data();
    view.planes[0].size = data->size();
    view.planes[0].rowStride = static_cast<uint32_t>(mat.cols * mat.elemSize());
    view.owner = data;
    return true;
}

} // namespace svs
} // namespace bsp_perf
