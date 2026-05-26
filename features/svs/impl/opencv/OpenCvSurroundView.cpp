#include "svs/SurroundView.hpp"
#include <opencv2/imgproc.hpp>

namespace bsp_perf
{
namespace svs
{

int OpenCvSurroundView::setup(const SurroundViewConfig& config)
{
    m_config = config;

    for (size_t i = 0; i < kCameraCount; ++i) {
        const auto& cameraConfig = m_config.cameras[i];
        auto& cameraParams = m_cameraParams[i];
        cameraParams.name = cameraConfig.name;

        if (!m_paramLoader.loadCameraParameters(resolvePath(cameraConfig.calibPath), cameraParams)) {
            m_ready = false;
            return -1;
        }
    }

    if (!m_blender.setup(m_config)) {
        m_ready = false;
        return -1;
    }

    m_ready = true;
    return 0;
}

int OpenCvSurroundView::process(const FrameSet& input, OutputFrame& output)
{
    if (!m_ready) {
        return -1;
    }

    std::array<cv::Mat, kCameraCount> cameras = input.cameras;
    if (m_config.enableAwb || m_config.enableLuminanceBalance) {
        m_blender.applyAwbAndLuminanceBalance(cameras);
    }

    std::array<cv::Mat, kCameraCount> projectedImages;
    for (size_t i = 0; i < kCameraCount; ++i) {
        if (!m_projector.project(cameras[i], m_cameraParams[i], m_config.cameras[i], m_config.layout, projectedImages[i])) {
            return -1;
        }
    }

    return m_blender.blend(projectedImages, output) ? 0 : -1;
}

int OpenCvSurroundView::tearDown()
{
    m_blender.reset();
    for (auto& cameraParam : m_cameraParams) {
        cameraParam = CameraParameters{};
    }
    m_ready = false;
    return 0;
}

std::string OpenCvSurroundView::resolvePath(const std::string& path) const
{
    if (path.empty() || m_config.dataRoot.empty() || path.front() == '/') {
        return path;
    }
    if (m_config.dataRoot.back() == '/') {
        return m_config.dataRoot + path;
    }
    return m_config.dataRoot + "/" + path;
}

} // namespace svs
} // namespace bsp_perf
