#ifndef __RGA_UTILS_HPP__
#define __RGA_UTILS_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_g2d/impl/G2DBufferInternal.hpp>
#include <rga/im2d.h>
#include <rga/im2d_common.h>

#include <any>
#include <cstdint>
#include <iostream>
#include <memory>

namespace bsp_g2d
{
namespace rga_detail
{

inline bool isOk(IM_STATUS status)
{
    return status == IM_STATUS_SUCCESS || status == IM_STATUS_NOERROR;
}

inline int normalizeStatus(IM_STATUS status, const char* op)
{
    if (isOk(status)) {
        return 0;
    }
    std::cerr << "rkrga: " << op << " failed: " << imStrError_t(status)
              << " (" << static_cast<int>(status) << ")" << std::endl;
    return -1;
}

inline int setScheduler(uint64_t scheduler)
{
    if (scheduler == IM_SCHEDULER_DEFAULT) {
        std::cerr << "rkrga: refusing invalid default scheduler core mask" << std::endl;
        return -1;
    }
    return normalizeStatus(
        static_cast<IM_STATUS>(imconfig(IM_CONFIG_SCHEDULER_CORE, scheduler)),
        "setScheduler imconfig");
}

inline rga_buffer_t getRgaBuffer(const std::shared_ptr<impl::G2DBufferInternal>& buffer)
{
    if (!buffer || !buffer->g2dBufferHandle.has_value()) {
        return {};
    }
    return std::any_cast<rga_buffer_t>(buffer->g2dBufferHandle);
}

inline im_rect toImRect(const IGraphics2D::ImageRect& rect)
{
    im_rect out{};
    if (rect.width > 0 && rect.height > 0) {
        out.x = rect.x;
        out.y = rect.y;
        out.width = rect.width;
        out.height = rect.height;
    }
    return out;
}

inline int toInterpolation(IGraphics2D::Interpolation interpolation)
{
    switch (interpolation) {
    case IGraphics2D::Interpolation::Linear:
        return IM_INTERP_LINEAR;
    case IGraphics2D::Interpolation::Cubic:
        return IM_INTERP_CUBIC;
    case IGraphics2D::Interpolation::Average:
        return IM_INTERP_AVERAGE;
    case IGraphics2D::Interpolation::Default:
        break;
    }
    return IM_INTERP_DEFAULT;
}

inline int toTransformUsage(IGraphics2D::Rotation rotation, IGraphics2D::FlipMode flip)
{
    int usage = 0;
    switch (rotation) {
    case IGraphics2D::Rotation::Rotate90:
        usage |= IM_HAL_TRANSFORM_ROT_90;
        break;
    case IGraphics2D::Rotation::Rotate180:
        usage |= IM_HAL_TRANSFORM_ROT_180;
        break;
    case IGraphics2D::Rotation::Rotate270:
        usage |= IM_HAL_TRANSFORM_ROT_270;
        break;
    case IGraphics2D::Rotation::Rotate0:
        break;
    }

    switch (flip) {
    case IGraphics2D::FlipMode::Horizontal:
        usage |= IM_HAL_TRANSFORM_FLIP_H;
        break;
    case IGraphics2D::FlipMode::Vertical:
        usage |= IM_HAL_TRANSFORM_FLIP_V;
        break;
    case IGraphics2D::FlipMode::Both:
        usage |= IM_HAL_TRANSFORM_FLIP_H_V;
        break;
    case IGraphics2D::FlipMode::None:
        break;
    }
    return usage;
}

inline int toBlendMode(IGraphics2D::BlendMode mode)
{
    switch (mode) {
    case IGraphics2D::BlendMode::Src:
        return IM_ALPHA_BLEND_SRC | IM_ALPHA_BLEND_PRE_MUL;
    case IGraphics2D::BlendMode::Dst:
        return IM_ALPHA_BLEND_DST | IM_ALPHA_BLEND_PRE_MUL;
    case IGraphics2D::BlendMode::DstOver:
        return IM_ALPHA_BLEND_DST_OVER | IM_ALPHA_BLEND_PRE_MUL;
    case IGraphics2D::BlendMode::SrcOver:
        break;
    }
    return IM_ALPHA_BLEND_SRC_OVER | IM_ALPHA_BLEND_PRE_MUL;
}

} // namespace rga_detail
} // namespace bsp_g2d

#endif // __RGA_UTILS_HPP__
