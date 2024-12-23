#include "rkrga.hpp"
#include <iostream>
#include <memory>
#include <any>
#include <rga/im2d.h>
#include <rga/rga.h>
#include <rga/RgaUtils.h>


namespace bsp_g2d
{

rkrga::rkrga()
{
    std::cout << "rkrga constructor" << std::endl;
}

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createG2DBuffer(const std::string& g2dBufferMapType, G2DBufferParams& params)
{
    std::shared_ptr<G2DBuffer> g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->g2dPlatform = "rkrga";
    int rga_format = m_rgaParams.m_pix_format_map.at(params.format);

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
        if (handle == (rga_buffer_handle_t) nullptr)
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
    return imresize(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle));
}

int rkrga::imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    return imcopy(std::any_cast<rga_buffer_t>(src->g2dBufferHandle), std::any_cast<rga_buffer_t>(dst->g2dBufferHandle));
}


} // namespace bsp_g2d