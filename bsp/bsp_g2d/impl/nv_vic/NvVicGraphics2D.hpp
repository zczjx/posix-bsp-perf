#ifndef __NV_VIC_GRAPHICS2D_HPP__
#define __NV_VIC_GRAPHICS2D_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
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
 * - Color format conversion
 * - Image scaling/resizing
 * - Image copying
 * 
 * Based on the nvbufsurface and nvbufsurftransform APIs.
 */
class NvVicGraphics2D : public IGraphics2D
{
public:
    NvVicGraphics2D();
    virtual ~NvVicGraphics2D();

    /**
     * @brief Creates a G2D buffer for NV VIC operations.
     * 
     * @param g2dBufferMapType The type of G2D buffer mapping ("fd", "virtualaddr", "physicaladdr", "handle")
     * @param params The parameters for the G2D buffer
     * @return std::shared_ptr<G2DBuffer> A shared pointer to the created G2D buffer
     */
    std::shared_ptr<G2DBuffer> createG2DBuffer(
        const std::string& g2dBufferMapType, 
        G2DBufferParams& params) override;

    /**
     * @brief Releases a G2D buffer.
     * 
     * @param g2dBuffer The G2D buffer to release
     */
    void releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer) override;

    /**
     * @brief Resizes an image using NV VIC hardware acceleration.
     * 
     * @param src Source G2D buffer
     * @param dst Destination G2D buffer (can have different dimensions)
     * @return int 0 on success, negative on error
     */
    int imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

    /**
     * @brief Copies an image using NV VIC.
     * 
     * @param src Source G2D buffer
     * @param dst Destination G2D buffer
     * @return int 0 on success, negative on error
     */
    int imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

    /**
     * @brief Draws a rectangle on the image.
     * 
     * Note: VIC hardware does not support drawing primitives directly.
     * This function is not implemented and will return an error.
     * 
     * @param dst Destination G2D buffer
     * @param rect Rectangle coordinates
     * @param color Color in RGBA format
     * @param thickness Line thickness
     * @return int -1 (not supported)
     */
    int imageDrawRectangle(
        std::shared_ptr<G2DBuffer> dst, 
        ImageRect& rect, 
        uint32_t color, 
        int thickness) override;

    /**
     * @brief Converts color format using NV VIC hardware acceleration.
     * 
     * Supported conversions include:
     * - YUV <-> RGB variants
     * - Different YUV formats (NV12, YUV420, etc.)
     * 
     * @param src Source G2D buffer
     * @param dst Destination G2D buffer
     * @param src_format Source format string (must match params.format)
     * @param dst_format Destination format string (must match params.format)
     * @return int 0 on success, negative on error
     */
    int imageCvtColor(
        std::shared_ptr<G2DBuffer> src, 
        std::shared_ptr<G2DBuffer> dst,
        const std::string& src_format, 
        const std::string& dst_format) override;

private:
    /**
     * @brief Maps format string to NvBufSurfaceColorFormat.
     * 
     * @param format Format string from G2DBufferParams
     * @return NvBufSurfaceColorFormat Corresponding NvBufSurfaceColorFormat
     */
    NvBufSurfaceColorFormat mapFormatStringToNvFormat(const std::string& format);

    /**
     * @brief Gets NvBufSurface from G2DBuffer.
     * 
     * @param g2dBuffer Input G2D buffer
     * @return NvBufSurface* Pointer to NvBufSurface
     */
    NvBufSurface* getNvBufSurface(std::shared_ptr<G2DBuffer> g2dBuffer);

    /**
     * @brief Performs NV VIC transformation.
     * 
     * @param src Source NvBufSurface
     * @param dst Destination NvBufSurface
     * @param transform_params Transform parameters
     * @return int 0 on success, negative on error
     */
    int performTransform(
        NvBufSurface* src, 
        NvBufSurface* dst,
        NvBufSurfTransformParams& transform_params);

    /**
     * @brief Fills bytes per pixel array for a given format.
     * 
     * @param pixel_format NvBufSurfaceColorFormat
     * @param bytes_per_pixel Output array for bytes per pixel per plane
     */
    void fillBytesPerPixel(NvBufSurfaceColorFormat pixel_format, int* bytes_per_pixel);

    // Store NvBufSurface pointers for each G2DBuffer
    std::map<void*, NvBufSurface*> m_bufferMap;
    std::mutex m_mapMutex;
    
    // VIC session initialized flag
    bool m_sessionInitialized;
};

} // namespace bsp_g2d

#endif // __NV_VIC_GRAPHICS2D_HPP__

