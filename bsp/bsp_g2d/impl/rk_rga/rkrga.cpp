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

std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> rkrga::createBuffer(
    BufferType type,
    const bsp_perf::bsp_image::ImageView& image)
{
    auto imageBuffer = std::make_shared<bsp_perf::bsp_image::ImageBuffer>();
    imageBuffer->view = image;
    auto g2dBuffer = std::make_shared<impl::G2DBufferInternal>();
    g2dBuffer->g2dPlatform = "rkrga";
    g2dBuffer->bufferType = type;
    imageBuffer->nativeHandle = g2dBuffer;

    const auto& desc = image.desc;
    const auto& plane = image.planes[0];
    int rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(desc.format);
    const uint32_t widthStride = desc.widthStride > 0 ? desc.widthStride : desc.width;
    const uint32_t heightStride = desc.heightStride > 0 ? desc.heightStride : desc.height;

    if (type == BufferType::Hardware)
    {
        // Hardware buffer: use fd or handle
        if (plane.fd >= 0) {
            g2dBuffer->g2dBufferHandle = wrapbuffer_fd_t(
                plane.fd,
                desc.width,
                desc.height,
                widthStride,
                heightStride,
                rga_format);
        } else {
            std::cerr << "rkrga: Hardware buffer requires fd" << std::endl;
            return nullptr;
        }
    }
    else if (type == BufferType::Mapped)
    {
        if (plane.data == nullptr)
        {
            std::cerr << "rkrga: Mapped buffer requires host_ptr" << std::endl;
            return nullptr;
        }

        g2dBuffer->hostPtr = plane.data;
        g2dBuffer->bufferSize = desc.dataSize;
        g2dBuffer->g2dBufferHandle = wrapbuffer_virtualaddr_t(
            plane.data, desc.width, desc.height,
            widthStride, heightStride, rga_format);
    }
    else
    {
        return nullptr;
    }

    return imageBuffer;
}

void rkrga::releaseBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer)
{
    if (buffer == nullptr)
    {
        return;
    }
    // RGA buffers don't need explicit cleanup for wrapped buffers
    // The user is responsible for managing the underlying memory
    buffer.reset();
}

int rkrga::syncBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer, SyncDirection direction)
{
    // RGA with virtualaddr doesn't need explicit sync
    // The driver handles cache coherency automatically
    // This is a no-op for RGA
    return 0;
}

void* rkrga::mapBuffer(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer,
    const std::string& access_mode)
{
    auto g2dBuffer = impl::getG2DBufferInternal(buffer);
    if (!g2dBuffer || g2dBuffer->bufferType != BufferType::Hardware) {
        std::cerr << "rkrga: mapBuffer only works with Hardware buffers" << std::endl;
        return nullptr;
    }

    // For RGA, mapping hardware buffers is not commonly supported in the same way as VIC
    // This would require platform-specific implementation
    std::cerr << "rkrga: mapBuffer not implemented for RGA hardware buffers" << std::endl;
    return nullptr;
}

void rkrga::unmapBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer)
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

// ========== Image Operations ==========

int rkrga::imageResize(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    // WAR: Set rga core affinity for each thread to avoid RGA2 core3 being scheduled
    static thread_local bool rga_core_affinity_set = false;
    if (!rga_core_affinity_set)
    {
        imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE1);
        rga_core_affinity_set = true;
    }

    im_rect src_rect{};
    im_rect dst_rect{};
    int ret = imcheck(std::any_cast<rga_buffer_t>(srcBuffer->g2dBufferHandle), std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle), src_rect, dst_rect);
    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imcheck failed" << std::endl;
        return ret;
    }

    ret = imresize(std::any_cast<rga_buffer_t>(srcBuffer->g2dBufferHandle), std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle));
    if (ret == IM_STATUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        std::cerr << "imageResize imresize failed ret: " << ret << std::endl;
        return -1;
    }
}

int rkrga::imageCopy(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    int ret = imcopy(std::any_cast<rga_buffer_t>(srcBuffer->g2dBufferHandle), std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle));
    if (ret == IM_STATUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        std::cerr << "imageCopy imcopy failed ret: " << ret << std::endl;
        return -1;
    }
}

int rkrga::imageDrawRectangle(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst, ImageRect& rect, uint32_t color, int thickness)
{
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!dstBuffer) {
        return -1;
    }
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
    int ret = imcheck({}, std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle), {}, dst_rect, IM_COLOR_FILL);

    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imageDrawRectangle imcheck failed" << std::endl;
        return ret;
    }
    ret = imrectangle(std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle), dst_rect, color, thickness);

    if (ret == IM_STATUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        std::cerr << "imageDrawRectangle imrectangle failed ret: " << ret << std::endl;
        return -1;
    }
}

int rkrga::imageCvtColor(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
                    const std::string& src_format, const std::string& dst_format)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    // WAR: Set rga core affinity for each thread to avoid RGA2 core3 being scheduled
    static thread_local bool rga_core_affinity_set = false;
    if (!rga_core_affinity_set)
    {
        imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_CORE::IM_SCHEDULER_RGA3_CORE1);
        rga_core_affinity_set = true;
    }

    int src_rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(src_format);
    int dst_rga_format = rgaPixelFormat::getInstance().strToRgaPixFormat(dst_format);

    int ret = imcheck(std::any_cast<rga_buffer_t>(srcBuffer->g2dBufferHandle), std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle), {}, {});

    if (ret != IM_STATUS_NOERROR)
    {
        std::cerr << "imageCvtColor imcheck failed ret: " << ret << std::endl;
        return ret;
    }

    ret = imcvtcolor(std::any_cast<rga_buffer_t>(srcBuffer->g2dBufferHandle), std::any_cast<rga_buffer_t>(dstBuffer->g2dBufferHandle),
                src_rga_format, dst_rga_format);

    if (ret == IM_STATUS_SUCCESS)
    {
        return 0;
    }
    else
    {
        std::cerr << "imageCvtColor imcvtcolor failed ret: " << ret << std::endl;
        return -1;
    }
}

} // namespace bsp_g2d
