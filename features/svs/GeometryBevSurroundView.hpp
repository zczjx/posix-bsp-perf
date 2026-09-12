#ifndef __GEOMETRY_BEV_SURROUND_VIEW_HPP__
#define __GEOMETRY_BEV_SURROUND_VIEW_HPP__

#include "ISurroundView.hpp"
#include "SvsBlender.hpp"
#include "SvsProjector.hpp"

namespace bsp_perf
{
namespace svs
{

class GeometryBevSurroundView : public ISurroundView
{
public:
    int setup(const SurroundViewConfig& config) override;
    int process(const FrameSet& input, OutputFrame& output) override;
    int tearDown() override;

private:
    SvsProjector m_projector;
    SvsBlender m_blender;
    SurroundViewConfig m_config;
    bool m_ready{false};
};

} // namespace svs
} // namespace bsp_perf

#endif // __GEOMETRY_BEV_SURROUND_VIEW_HPP__
