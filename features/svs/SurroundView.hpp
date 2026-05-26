#ifndef __SURROUND_VIEW_HPP__
#define __SURROUND_VIEW_HPP__

#include "ISurroundView.hpp"
#include "SvsBlender.hpp"
#include "SvsParamLoader.hpp"
#include "SvsProjector.hpp"

namespace bsp_perf
{
namespace svs
{

class OpenCvSurroundView : public ISurroundView
{
public:
    int setup(const SurroundViewConfig& config) override;
    int process(const FrameSet& input, OutputFrame& output) override;
    int tearDown() override;

private:
    std::string resolvePath(const std::string& path) const;

private:
    SurroundViewConfig m_config;
    SvsParamLoader m_paramLoader;
    SvsProjector m_projector;
    SvsBlender m_blender;
    std::array<CameraParameters, kCameraCount> m_cameraParams;
    bool m_ready{false};
};

} // namespace svs
} // namespace bsp_perf

#endif // __SURROUND_VIEW_HPP__
