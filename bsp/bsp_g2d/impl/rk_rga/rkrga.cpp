#include "rkrga.hpp"
#include <rga/im2d_common.h>
#include <iostream>
#include <memory>
#include <any>
#include <string.h>
#include <thread>

namespace bsp_g2d
{

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createG2DBuffer(const std::string& g2dBufferMapType, G2DBufferParams& params)
{
    std::shared_ptr<G2DBuffer> g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->g2dPlatform = "rkrga";
    int rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(params.format);

    if (g2dBufferMapType.compare("fd") == 0)
    {
        if(params.fd < 0)
        {
            return nullptr;
        }
        g2dBuffer->g2dBufferHandle = wrapbuffer_fd_t(params.fd, params.width, params.height, params.width_stride, params.height_stride, rga_format);
    }
    else if (g2dBufferMapType.compare("virtualaddr") == 0)
    {
        if(params.virtual_addr == nullptr)
        {
            return nullptr;
        }
        g2dBuffer->rawBuffer = static_cast<uint8_t*>(params.virtual_addr);
        g2dBuffer->rawBufferSize = params.rawBufferSize;
        g2dBuffer->g2dBufferHandle = wrapbuffer_virtualaddr_t(params.virtual_addr, params.width, params.height, params.width_stride, params.height_stride, rga_format);
    }
    else if (g2dBufferMapType.compare("physicaladdr") == 0)
    {
        if(params.physical_addr == nullptr)
        {
            return nullptr;
        }
        g2dBuffer->rawBuffer = static_cast<uint8_t*>(params.physical_addr);
        g2dBuffer->rawBufferSize = params.rawBufferSize;
        g2dBuffer->g2dBufferHandle = wrapbuffer_physicaladdr_t(params.physical_addr, params.width, params.height, params.width_stride, params.height_stride, rga_format);
    }
    else if (g2dBufferMapType.compare("handle") == 0)
    {
        rga_buffer_handle_t handle = std::any_cast<rga_buffer_handle_t>(params.handle);

        if (handle == NULL)
        {
            return nullptr;
        }
        g2dBuffer->g2dBufferHandle = wrapbuffer_handle_t(handle, params.width, params.height, params.width_stride, params.height_stride, rga_format);
    }
    else
    {
        return nullptr;
    }

    return g2dBuffer;
}

void rkrga::releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer)
{
    if(g2dBuffer == nullptr)
    {
        return;
    }

    if((g2dBuffer->rawBuffer != nullptr) && (g2dBuffer->rawBufferSize > 0))
    {
        free(g2dBuffer->rawBuffer);
        g2dBuffer->rawBuffer = nullptr;
        g2dBuffer->rawBufferSize = 0;
    }

    g2dBuffer.reset();
}

int rkrga::imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    // WAR: Set rga core affinity for each thread to avoid RGA2 core3 being scheduled
    static thread_local bool rga_core_affinity_set = false;
    if (!rga_core_affinity_set)
    {
        imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE1);
        rga_core_affinity_set = true;
    }

    im_rect src_rect{};
    im_rect dst_rect{};
    int ret = imcheck(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle), src_rect, dst_rect);
    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imcheck failed" << std::endl;
        return ret;
    }
    return imresize(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle));
}

int rkrga::imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    return imcopy(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle));
}

int rkrga::imageDrawRectangle(std::shared_ptr<G2DBuffer> dst, ImageRect& rect, uint32_t color, int thickness)
{
    // WAR: Set rga core affinity for each thread to avoid RGA2 core3 being scheduled
    static thread_local bool rga_core_affinity_set = false;
    if (!rga_core_affinity_set)
    {
        imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE1);
        rga_core_affinity_set = true;
    }

    im_rect dst_rect = {};
    dst_rect.x = rect.x;
    dst_rect.y = rect.y;
    dst_rect.width = rect.width;
    dst_rect.height = rect.height;
    int ret = imcheck({}, std::any_cast<rga_buffer_t>(dst->g2dBufferHandle), {}, dst_rect, IM_COLOR_FILL);

    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imageDrawRectangle imcheck failed" << std::endl;
        return ret;
    }
    return imrectangle(std::any_cast<rga_buffer_t>(dst->g2dBufferHandle), dst_rect, color, thickness);
}

int rkrga::imageCvtColor(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst,
                    const std::string& src_format, const std::string& dst_format)
{
    // WAR: Set rga core affinity for each thread to avoid RGA2 core3 being scheduled
    static thread_local bool rga_core_affinity_set = false;
    if (!rga_core_affinity_set)
    {
        imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE1);
        rga_core_affinity_set = true;
    }

    int src_rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(src_format);
    int dst_rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(dst_format);

    int ret = imcheck(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle), {}, {});

    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imageCvtColor imcheck failed ret: " << ret << std::endl;
        return ret;
    }

    return imcvtcolor(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle),
                src_rga_format, dst_rga_format);
}


} // namespace bsp_g2d