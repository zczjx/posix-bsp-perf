#include "rkrga.hpp"
#include <rga/im2d_common.h>
#include <iostream>
#include <memory>
#include <any>
#include <string.h>
#include <thread>

namespace bsp_g2d
{

// ========== New Interface Implementation ==========

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createBuffer(
    BufferType type,
    const G2DBufferParams& params)
{
    std::shared_ptr<G2DBuffer> g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->g2dPlatform = "rkrga";
    g2dBuffer->bufferType = type;
    
    int rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(params.format);
    
    if (type == BufferType::Hardware) {
        // Hardware buffer: use fd or handle
        if (params.fd >= 0) {
            g2dBuffer->g2dBufferHandle = wrapbuffer_fd_t(
                params.fd, 
                params.width, 
                params.height, 
                params.width_stride > 0 ? params.width_stride : params.width, 
                params.height_stride > 0 ? params.height_stride : params.height, 
                rga_format);
        } else if (params.handle.has_value()) {
            rga_buffer_handle_t handle = std::any_cast<rga_buffer_handle_t>(params.handle);
            if (handle != NULL) {
                g2dBuffer->g2dBufferHandle = wrapbuffer_handle_t(
                    handle, 
                    params.width, 
                    params.height, 
                    params.width_stride > 0 ? params.width_stride : params.width, 
                    params.height_stride > 0 ? params.height_stride : params.height, 
                    rga_format);
            } else {
                return nullptr;
            }
        } else {
            std::cerr << "rkrga: Hardware buffer requires fd or handle" << std::endl;
            return nullptr;
        }
    } else if (type == BufferType::Mapped) {
        // Mapped buffer: use virtualaddr
        void* host_ptr = params.host_ptr;
        size_t buf_size = params.buffer_size;
        
        // Support legacy field names
        if (!host_ptr && params.virtual_addr) {
            host_ptr = params.virtual_addr;
        }
        if (buf_size == 0 && params.rawBufferSize > 0) {
            buf_size = params.rawBufferSize;
        }
        
        if (host_ptr == nullptr) {
            std::cerr << "rkrga: Mapped buffer requires host_ptr" << std::endl;
            return nullptr;
        }
        
        g2dBuffer->host_ptr = static_cast<uint8_t*>(host_ptr);
        g2dBuffer->buffer_size = buf_size;
        
        g2dBuffer->g2dBufferHandle = wrapbuffer_virtualaddr_t(
            host_ptr, 
            params.width, 
            params.height, 
            params.width_stride > 0 ? params.width_stride : params.width, 
            params.height_stride > 0 ? params.height_stride : params.height, 
            rga_format);
    } else {
        return nullptr;
    }
    
    return g2dBuffer;
}

void rkrga::releaseBuffer(std::shared_ptr<G2DBuffer> buffer)
{
    if (buffer == nullptr) {
        return;
    }
    
    // RGA buffers don't need explicit cleanup for wrapped buffers
    // The user is responsible for managing the underlying memory
    
    buffer.reset();
}

int rkrga::syncBuffer(
    std::shared_ptr<G2DBuffer> buffer,
    SyncDirection direction)
{
    // RGA with virtualaddr doesn't need explicit sync
    // The driver handles cache coherency automatically
    // This is a no-op for RGA
    return 0;
}

void* rkrga::mapBuffer(
    std::shared_ptr<G2DBuffer> buffer,
    const std::string& access_mode)
{
    if (!buffer || buffer->bufferType != BufferType::Hardware) {
        std::cerr << "rkrga: mapBuffer only works with Hardware buffers" << std::endl;
        return nullptr;
    }
    
    // For RGA, mapping hardware buffers is not commonly supported in the same way as VIC
    // This would require platform-specific implementation
    std::cerr << "rkrga: mapBuffer not implemented for RGA hardware buffers" << std::endl;
    return nullptr;
}

void rkrga::unmapBuffer(std::shared_ptr<G2DBuffer> buffer)
{
    // No-op for RGA
}

bool rkrga::queryCapability(const std::string& capability) const
{
    if (capability == "hardware_draw") return true;
    if (capability == "zero_copy_cpu_access") return true;
    if (capability == "requires_explicit_sync") return false;
    return false;
}

std::string rkrga::getPlatformName() const
{
    return "rkrga";
}

// ========== Legacy Interface Implementation ==========

std::shared_ptr<IGraphics2D::G2DBuffer> rkrga::createG2DBuffer(
    const std::string& g2dBufferMapType, 
    G2DBufferParams& params)
{
    // Convert legacy parameters to new interface
    BufferType type = BufferType::Hardware;
    
    if (g2dBufferMapType == "virtualaddr") {
        type = BufferType::Mapped;
        if (!params.host_ptr && params.virtual_addr) {
            params.host_ptr = params.virtual_addr;
        }
        if (params.buffer_size == 0 && params.rawBufferSize > 0) {
            params.buffer_size = params.rawBufferSize;
        }
    } else if (g2dBufferMapType == "fd" || g2dBufferMapType == "handle" || 
               g2dBufferMapType == "physicaladdr") {
        type = BufferType::Hardware;
    }
    
    auto buffer = createBuffer(type, params);
    return buffer;
}

void rkrga::releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer)
{
    releaseBuffer(g2dBuffer);
}

// ========== Image Operations ==========

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
