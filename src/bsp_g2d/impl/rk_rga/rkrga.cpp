#include "rkrga.hpp"
#include <iostream>
#include <memory>

namespace bsp_g2d
{

rkrga::rkrga()
{
    std::cout << "rkrga constructor" << std::endl;
}

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createG2DBuffer(void* vir_addr, size_t rawBufferSize, size_t width, size_t height, const std::string& format)
{
    if(vir_addr == nullptr)
    {
        return nullptr;
    }

    std::shared_ptr<G2DBuffer> g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->handleType = "rkrga";
    g2dBuffer->rawBuffer = static_cast<uint8_t*>(vir_addr);
    g2dBuffer->rawBufferSize = rawBufferSize;
    int rga_format = m_rgaParams.m_pix_format_map.at(format);
    g2dBuffer->g2dBufferHandle = wrapbuffer_virtualaddr_t(vir_addr, width, height, width, height, rga_format);
    return g2dBuffer;

}

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createG2DBuffer(void* vir_addr, size_t rawBufferSize, size_t width, size_t height, const std::string& format,
                                            int width_stride, int height_stride)
{
    if(vir_addr == nullptr)
    {
        return nullptr;
    }

    std::shared_ptr<G2DBuffer> g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->handleType = "rkrga";
    g2dBuffer->rawBuffer = static_cast<uint8_t*>(vir_addr);
    g2dBuffer->rawBufferSize = rawBufferSize;
    int rga_format = m_rgaParams.m_pix_format_map.at(format);
    g2dBuffer->g2dBufferHandle = wrapbuffer_virtualaddr_t(vir_addr, width, height, width_stride, height_stride, rga_format);
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


} // namespace bsp_g2d