#ifndef __SVS_PROJECTOR_HPP__
#define __SVS_PROJECTOR_HPP__

#include "SvsTypes.hpp"
#include <array>
#include <opencv2/core.hpp>

namespace bsp_perf
{
namespace svs
{

class SvsProjector
{
public:
    bool setup(const SurroundViewConfig& config);
    bool project(size_t cameraIndex, const cv::Mat& source, cv::Mat& warped, cv::Mat& weight);
    void reset();

private:
    struct WarpCache
    {
        int sourceWidth{0};
        int sourceHeight{0};
        cv::Mat homography;
        cv::Mat weight;
    };

    bool buildWarp(size_t cameraIndex, int sourceWidth, int sourceHeight, WarpCache& cache) const;
    cv::Point2f vehicleToBev(double x, double y) const;

private:
    SurroundViewConfig m_config;
    std::array<WarpCache, kCameraCount> m_cache;
    bool m_ready{false};
};

} // namespace svs
} // namespace bsp_perf

#endif // __SVS_PROJECTOR_HPP__
