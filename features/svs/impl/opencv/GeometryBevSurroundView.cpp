#include "svs/GeometryBevSurroundView.hpp"

#include <array>
#include <bsp_image/OpenCvImageAdapter.hpp>

namespace bsp_perf
{
namespace svs
{

int GeometryBevSurroundView::setup(const SurroundViewConfig& config)
{
    tearDown();
    if (config.outputPixelFormat != "RGB888" && config.outputPixelFormat != "RGBA8888") {
        return -1;
    }
    if (!m_projector.setup(config) || !m_blender.setup(config)) {
        tearDown();
        return -1;
    }

    m_config = config;
    m_ready = true;
    return 0;
}

int GeometryBevSurroundView::process(const FrameSet& input, OutputFrame& output)
{
    if (!m_ready) {
        return -1;
    }

    std::array<cv::Mat, kCameraCount> sourceImages;
    std::array<cv::Mat, kCameraCount> warpedImages;
    std::array<cv::Mat, kCameraCount> weights;
    for (size_t i = 0; i < kCameraCount; ++i) {
        if (!bsp_perf::bsp_image::OpenCvImageAdapter::toRgbMat(input.cameras[i], sourceImages[i]) ||
            !m_projector.project(i, sourceImages[i], warpedImages[i], weights[i])) {
            return -1;
        }
    }

    cv::Mat blended;
    if (!m_blender.blend(warpedImages, weights, blended)) {
        return -1;
    }

    if (m_config.outputPixelFormat == "RGBA8888") {
        cv::Mat rgba;
        cv::cvtColor(blended, rgba, cv::COLOR_RGB2RGBA);
        blended = std::move(rgba);
    }

    return bsp_perf::bsp_image::OpenCvImageAdapter::fromMat(
               blended, m_config.outputPixelFormat, output.image)
               ? 0
               : -1;
}

int GeometryBevSurroundView::tearDown()
{
    m_projector.reset();
    m_blender.reset();
    m_ready = false;
    return 0;
}

} // namespace svs
} // namespace bsp_perf
