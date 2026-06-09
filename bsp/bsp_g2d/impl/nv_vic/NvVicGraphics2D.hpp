#ifndef __NV_VIC_GRAPHICS2D_HPP__
#define __NV_VIC_GRAPHICS2D_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_g2d/impl/G2DBufferInternal.hpp>
#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include <map>
#include <mutex>

namespace bsp_g2d
{

/**
 * @brief NV VIC implementation of IGraphics2D interface.
 * 
 * This class provides hardware-accelerated 2D graphics operations using
 * NVIDIA VIC (Video Image Compositor) engine. It supports:
 * - Color format conversion (YUV formats and 32-bit RGB formats)
 * - Image scaling/resizing
 * - Image copying
 * 
 * ⚠️ IMPORTANT FORMAT LIMITATIONS:
 * - VIC hardware does NOT support 24-bit RGB/BGR formats (RGB888/BGR888)
 * - Only 32-bit RGB formats are supported (RGBA8888, BGRA8888, ARGB8888, etc.)
 * - If you need RGB888/BGR888, use RGBA8888 instead or use GPU-based transformation
 * 
 * Supported color format conversions:
 * - YUV formats: YUV420SP (NV12), YUV420P, YUV422, YUV444, etc.
 * - 32-bit RGB: RGBA8888, BGRA8888, ARGB8888, ABGR8888
 * - YUV ↔ 32-bit RGB conversions
 * 
 * Based on the nvbufsurface and nvbufsurftransform APIs.
 */
class NvVicGraphics2D : public IGraphics2D
{
public:
    NvVicGraphics2D();
    virtual ~NvVicGraphics2D();

    // ========== New Interface ==========

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

    int imageCopy(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst) override;

    int imageDrawRectangle(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst, ImageRect& rect,
            uint32_t color, int thickness) override;

    int imageCvtColor(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
            const std::string& src_format, const std::string& dst_format) override;

private:
    /**
     * @brief Maps format string to NvBufSurfaceColorFormat.
     */
    NvBufSurfaceColorFormat mapFormatStringToNvFormat(const std::string& format);

    /**
     * @brief Gets NvBufSurface from ImageBuffer.
     */
    NvBufSurface* getNvBufSurface(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> imageBuffer);

    /**
     * @brief Performs NV VIC transformation.
     */
    int performTransform(NvBufSurface* src, NvBufSurface* dst, NvBufSurfTransformParams& transform_params);

    /**
     * @brief Fills bytes per pixel array for a given format.
     */
    void fillBytesPerPixel(NvBufSurfaceColorFormat pixel_format, int* bytes_per_pixel);

    /**
     * @brief Copy data from host pointer to NvBufSurface.
     */
    int copyHostToNvBufSurface(void* host_ptr, size_t buffer_size, NvBufSurface* surf);

    /**
     * @brief Copy data from NvBufSurface to host pointer.
     */
    int copyNvBufSurfaceToHost(NvBufSurface* surf, void* host_ptr, size_t buffer_size);

    // Store NvBufSurface pointers for each ImageBuffer
    std::map<void*, NvBufSurface*> m_bufferMap;
    std::mutex m_mapMutex;
    
    // Store map state for Hardware buffers
    struct MapInfo {
        void* mapped_addr;
        bool is_mapped;
    };
    std::map<void*, MapInfo> m_mapInfo;
    
    // VIC session initialized flag
    bool m_sessionInitialized;
};

} // namespace bsp_g2d

#endif // __NV_VIC_GRAPHICS2D_HPP__
