#ifndef __OPEN_CV_IMAGE_ADAPTER_HPP__
#define __OPEN_CV_IMAGE_ADAPTER_HPP__

#include <opencv2/core.hpp>
#include <image/ImageTypes.hpp>

namespace bsp_perf
{
namespace svs
{

class OpenCvImageAdapter
{
public:
    static bool toMat(const bsp_perf::image::ImageView& view, cv::Mat& mat);
    static bool fromMat(const cv::Mat& mat, const std::string& format, bsp_perf::image::ImageView& view);
};

} // namespace svs
} // namespace bsp_perf

#endif // __OPEN_CV_IMAGE_ADAPTER_HPP__
