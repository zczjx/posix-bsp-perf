#include "rkrga.hpp"
#include "RgaJob.hpp"
#include "RgaUtils.hpp"
#include <rga/im2d_common.h>
#include <rga/im2d_task.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string.h>
#include <utility>

namespace bsp_g2d
{

using namespace rga_detail;

namespace
{

std::string makeImportKey(
    IGraphics2D::BufferType type,
    const bsp_perf::bsp_image::ImageView& image,
    RgaSURF_FORMAT format,
    uint32_t widthStride,
    uint32_t heightStride,
    size_t bufferSize)
{
    const auto& plane = image.planes[0];
    std::ostringstream os;
    os << static_cast<int>(type) << ':'
       << (type == IGraphics2D::BufferType::Hardware ? plane.fd : -1) << ':'
       << static_cast<const void*>(plane.data) << ':'
       << bufferSize << ':'
       << image.desc.width << 'x' << image.desc.height << ':'
       << widthStride << 'x' << heightStride << ':'
       << static_cast<int>(format);
    return os.str();
}

} // namespace

struct rkrga::ImportedBufferResource
{
    explicit ImportedBufferResource(rga_buffer_handle_t importedHandle)
        : handle(importedHandle)
    {
    }

    ~ImportedBufferResource()
    {
        if (handle) {
            releasebuffer_handle(handle);
        }
    }

    rga_buffer_handle_t handle{};
};

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
    RgaSURF_FORMAT rga_format{};
    if (!rgaPixelFormat::getInstance().tryStrToRgaPixFormat(desc.format, rga_format)) {
        std::cerr << "rkrga: unsupported pixel format: " << desc.format << std::endl;
        return nullptr;
    }
    const uint32_t widthStride = desc.widthStride > 0 ? desc.widthStride : desc.width;
    const uint32_t heightStride = desc.heightStride > 0 ? desc.heightStride : desc.height;
    auto imported = importBuffer(type, image, rga_format, widthStride, heightStride);
    if (!imported) {
        return nullptr;
    }
    g2dBuffer->platformData = imported;
    g2dBuffer->bufferSize = desc.dataSize > 0 ? desc.dataSize : bsp_perf::bsp_image::imageDataSize(desc);
    g2dBuffer->g2dBufferHandle = wrapbuffer_handle_t(
        imported->handle,
        desc.width,
        desc.height,
        widthStride,
        heightStride,
        rga_format);

    if (type == BufferType::Hardware)
    {
        if (plane.fd < 0) {
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
    if (capability == "rect_blit") return true;
    if (capability == "fill") return true;
    if (capability == "blend") return true;
    if (capability == "rotate") return true;
    if (capability == "flip") return true;
    if (capability == "async_job") return true;
    if (capability == "persistent_import") return true;
    return false;
}

std::string rkrga::getPlatformName() const
{
    return "rkrga";
}

// ========== Image Operations ==========

int rkrga::imageResize(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    TransformParams params{};
    return imageBlit(std::move(src), std::move(dst), params);
}

int rkrga::imageBlit(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const TransformParams& params)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }

    if (setScheduler(IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1) != 0) {
        return -1;
    }
    rga_buffer_t srcRga = getRgaBuffer(srcBuffer);
    rga_buffer_t dstRga = getRgaBuffer(dstBuffer);
    im_rect srcRect = toImRect(params.srcRect);
    im_rect dstRect = toImRect(params.dstRect);
    im_rect patRect{};
    rga_buffer_t pat{};
    im_opt_t opt{};
    opt.interp = toInterpolation(params.interpolation);
    const int usage = toTransformUsage(params.rotation, params.flip);

    IM_STATUS ret = imcheck(srcRga, dstRga, srcRect, dstRect, usage);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "imageBlit imcheck");
    }

    ret = improcess(srcRga, dstRga, pat, srcRect, dstRect, patRect, 0, nullptr, &opt, usage);
    return normalizeStatus(ret, "imageBlit improcess");
}

int rkrga::imageCrop(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const ImageRect& srcRect)
{
    TransformParams params{};
    params.srcRect = srcRect;
    return imageBlit(std::move(src), std::move(dst), params);
}

int rkrga::imageCopy(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    if (setScheduler(IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1) != 0) {
        return -1;
    }
    IM_STATUS ret = imcopy(getRgaBuffer(srcBuffer), getRgaBuffer(dstBuffer));
    return normalizeStatus(ret, "imageCopy imcopy");
}

int rkrga::imageDrawRectangle(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst, ImageRect& rect, uint32_t color, int thickness)
{
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!dstBuffer) {
        return -1;
    }
    if (setScheduler(IM_SCHEDULER_RGA2_CORE0) != 0) {
        return -1;
    }
    im_rect dstRect = toImRect(rect);
    IM_STATUS ret = imcheck({}, getRgaBuffer(dstBuffer), {}, dstRect, IM_COLOR_FILL);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "imageDrawRectangle imcheck");
    }
    ret = imrectangle(getRgaBuffer(dstBuffer), dstRect, color, thickness);
    return normalizeStatus(ret, "imageDrawRectangle imrectangle");
}

int rkrga::imageFill(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const ImageRect& rect,
    uint32_t color)
{
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!dstBuffer) {
        return -1;
    }
    if (setScheduler(IM_SCHEDULER_RGA2_CORE0) != 0) {
        return -1;
    }
    im_rect dstRect = toImRect(rect);
    IM_STATUS ret = imcheck({}, getRgaBuffer(dstBuffer), {}, dstRect, IM_COLOR_FILL);
    if (!isOk(ret)) {
        return normalizeStatus(ret, "imageFill imcheck");
    }
    ret = imfill(getRgaBuffer(dstBuffer), dstRect, color);
    return normalizeStatus(ret, "imageFill imfill");
}

int rkrga::imageBlend(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    BlendMode mode)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    if (setScheduler(IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1) != 0) {
        return -1;
    }
    IM_STATUS ret = imblend(getRgaBuffer(srcBuffer), getRgaBuffer(dstBuffer), toBlendMode(mode));
    return normalizeStatus(ret, "imageBlend imblend");
}

int rkrga::imageCvtColor(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
                    const std::string& src_format, const std::string& dst_format)
{
    auto srcBuffer = impl::getG2DBufferInternal(src);
    auto dstBuffer = impl::getG2DBufferInternal(dst);
    if (!srcBuffer || !dstBuffer) {
        return -1;
    }
    if (setScheduler(IM_SCHEDULER_RGA3_CORE0 | IM_SCHEDULER_RGA3_CORE1) != 0) {
        return -1;
    }

    RgaSURF_FORMAT src_rga_format{};
    RgaSURF_FORMAT dst_rga_format{};
    if (!rgaPixelFormat::getInstance().tryStrToRgaPixFormat(src_format, src_rga_format) ||
        !rgaPixelFormat::getInstance().tryStrToRgaPixFormat(dst_format, dst_rga_format)) {
        std::cerr << "rkrga: unsupported cvtColor format " << src_format
                  << " -> " << dst_format << std::endl;
        return -1;
    }

    IM_STATUS ret = imcheck(getRgaBuffer(srcBuffer), getRgaBuffer(dstBuffer), {}, {});

    if (!isOk(ret)) {
        return normalizeStatus(ret, "imageCvtColor imcheck");
    }

    ret = imcvtcolor(getRgaBuffer(srcBuffer), getRgaBuffer(dstBuffer), src_rga_format, dst_rga_format);
    return normalizeStatus(ret, "imageCvtColor imcvtcolor");
}

std::shared_ptr<IGraphics2DJob> rkrga::createJob()
{
    im_job_handle_t native = imbeginJob();
    if (native == 0) {
        std::cerr << "rkrga: imbeginJob failed" << std::endl;
        return nullptr;
    }
    return std::make_shared<RgaJob>(native);
}

std::shared_ptr<rkrga::ImportedBufferResource> rkrga::importBuffer(
    BufferType type,
    const bsp_perf::bsp_image::ImageView& image,
    RgaSURF_FORMAT format,
    uint32_t widthStride,
    uint32_t heightStride)
{
    const auto& desc = image.desc;
    const auto& plane = image.planes[0];
    const size_t bufferSize = desc.dataSize > 0 ? desc.dataSize : bsp_perf::bsp_image::imageDataSize(desc);
    const std::string key = makeImportKey(type, image, format, widthStride, heightStride, bufferSize);

    std::lock_guard<std::mutex> lock(m_importMutex);
    auto it = m_importCache.find(key);
    if (it != m_importCache.end()) {
        if (auto cached = it->second.lock()) {
            return cached;
        }
        m_importCache.erase(it);
    }

    rga_buffer_handle_t handle{};
    if (type == BufferType::Hardware) {
        if (plane.fd < 0) {
            return nullptr;
        }
        handle = importbuffer_fd(plane.fd, static_cast<int>(bufferSize));
    } else if (type == BufferType::Mapped) {
        if (plane.data == nullptr) {
            return nullptr;
        }
        handle = importbuffer_virtualaddr(plane.data, static_cast<int>(bufferSize));
    }

    if (!handle) {
        std::cerr << "rkrga: importbuffer failed" << std::endl;
        return nullptr;
    }

    auto resource = std::make_shared<ImportedBufferResource>(handle);
    m_importCache[key] = resource;
    return resource;
}

} // namespace bsp_g2d
