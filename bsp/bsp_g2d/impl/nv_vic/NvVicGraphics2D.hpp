#ifndef __NV_VIC_GRAPHICS2D_HPP__
#define __NV_VIC_GRAPHICS2D_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_g2d/impl/G2DBufferInternal.hpp>
#include "nvbufsurface.h"
#include "nvbufsurftransform.h"

namespace bsp_g2d
{

/**
 * @brief NV VIC implementation of IGraphics2D interface (Jetson Orin NX).
 *
 * Operations executed on the VIC hardware (NvBufSurfTransform):
 * - imageResize / imageCrop / imageBlit (scale, crop, dst rect, rotate 90/180/270, flip)
 * - imageCvtColor (YUV <-> 32-bit RGB)
 * - imageCopy (NvBufSurfaceCopy)
 *
 * Operations executed on the CPU (VIC has no such primitive). They work on the
 * mapped NvBufSurface, so they obey the same sync rules as VIC operations:
 * - imageFill / imageDrawRectangle (RGBA8888 / BGRA8888 only)
 * - imageBlend (Porter-Duff, straight alpha, RGBA8888 / BGRA8888 only)
 *
 * Jobs (createJob) are not supported: VIC has no job list, createJob() returns nullptr.
 *
 * Colors passed to imageFill / imageDrawRectangle are 0xAARRGGBB.
 *
 * ⚠️ FORMAT LIMITATIONS:
 * - VIC does NOT support 24-bit RGB/BGR (RGB888/BGR888); use RGBA8888 / BGRA8888.
 *
 * Buffers are owned by their ImageBuffer: the NvBufSurface is destroyed when the
 * last reference to the ImageBuffer goes away (or releaseBuffer() is called).
 *
 * Based on the nvbufsurface and nvbufsurftransform APIs.
 */
class NvVicGraphics2D : public IGraphics2D
{
public:
    NvVicGraphics2D();
    virtual ~NvVicGraphics2D();

    // ========== Buffer Management ==========

    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> createBuffer(
        BufferType type,
        const bsp_perf::bsp_image::ImageView& image) override;

    void releaseBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer) override;

    int syncBuffer(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer,
        SyncDirection direction) override;

    void* mapBuffer(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer,
        const std::string& access_mode = "readwrite") override;

    void unmapBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer) override;

    bool queryCapability(const std::string& capability) const override;

    std::string getPlatformName() const override;

    // ========== Image Operations ==========

    int imageResize(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst) override;

    int imageBlit(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const TransformParams& params) override;

    int imageCopy(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst) override;

    int imageDrawRectangle(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst, ImageRect& rect,
            uint32_t color, int thickness) override;

    int imageFill(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const ImageRect& rect,
        uint32_t color) override;

    int imageBlend(
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
        std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        BlendMode mode = BlendMode::SrcOver) override;

    int imageCvtColor(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
            const std::string& src_format, const std::string& dst_format) override;

private:
    // VIC session initialized flag
    bool m_sessionInitialized;
};

} // namespace bsp_g2d

#endif // __NV_VIC_GRAPHICS2D_HPP__
