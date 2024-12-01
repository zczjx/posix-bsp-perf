#ifndef __IGRAPHICS2D_HPP__
#define __IGRAPHICS2D_HPP__

#include <memory>
#include <string>
#include <any>

namespace bsp_g2d
{

class IGraphics2D
{
public:

    /**
     * @brief Factory function to create an instance of IGraphics2D.
     *
     * This static function creates and returns a unique pointer to an IGraphics2D
     * instance based on the specified graphics platform.
     *
     * @param g2dPlatform A string representing the graphics platform to be used.
     * @return std::unique_ptr<IGraphics2D> A unique pointer to the created IGraphics2D instance.
     */
    static std::unique_ptr<IGraphics2D> create(const std::string& g2dPlatform);

    struct G2DBuffer
    {
        /// default is rkrga, will add more types later
        std::string handleType{"rkrga"};
        /// any should be a pointer to buffer type
        std::any g2dBufferHandle;

        uint8_t* rawBuffer{nullptr};
        size_t rawBufferSize{0};
    };

    /**
     * @brief Creates a G2D buffer.
     *
     * This function creates a G2D buffer with the specified virtual address, width, height, and format.
     *
     * @param vir_addr A pointer to the virtual address of the buffer.
     * @param width The width of the buffer.
     * @param height The height of the buffer.
     * @param format The format of the buffer as a string. The possible values are:
     * - "RGBA_8888"
     * - "RGBX_8888"
     * - "RGB_888"
     * - "BGRA_8888"
     * - "RGB_565"
     * - "RGBA_5551"
     * - "RGBA_4444"
     * - "BGR_888"
     * - "YCbCr_422_SP"
     * - "YCbCr_422_P"
     * - "YCbCr_420_SP"
     * - "YCbCr_420_P"
     * - "YCrCb_422_SP"
     * - "YCrCb_422_P"
     * - "YCrCb_420_SP"
     * - "YCrCb_420_P"
     * - "BPP1"
     * - "BPP2"
     * - "BPP4"
     * - "BPP8"
     * - "Y4"
     * - "YCbCr_400"
     * - "BGRX_8888"
     * - "YVYU_422"
     * - "YVYU_420"
     * - "VYUY_422"
     * - "VYUY_420"
     * - "YUYV_422"
     * - "YUYV_420"
     * - "UYVY_422"
     * - "UYVY_420"
     * - "YCbCr_420_SP_10B"
     * - "YCrCb_420_SP_10B"
     * - "YCbCr_422_SP_10B"
     * - "YCrCb_422_SP_10B"
     * - "BGR_565"
     * - "BGRA_5551"
     * - "BGRA_4444"
     * - "ARGB_8888"
     * - "XRGB_8888"
     * - "ARGB_5551"
     * - "ARGB_4444"
     * - "ABGR_8888"
     * - "XBGR_8888"
     * - "ABGR_5551"
     * - "ABGR_4444"
     * - "RGBA2BPP"
     * - "A8"
     * - "YCbCr_444_SP"
     * - "YCrCb_444_SP"
     * - "Y8"
     * - "UNKNOWN"
     *
     * @return A shared pointer to the created G2D buffer.
     */
    virtual std::shared_ptr<G2DBuffer> createG2DBuffer(void* vir_addr, size_t rawBufferSize, size_t width, size_t height, const std::string& format) = 0;

    virtual std::shared_ptr<G2DBuffer> createG2DBuffer(void* vir_addr, size_t rawBufferSize, size_t width, size_t height, const std::string& format,
                                            int width_stride, int height_stride) = 0;
    virtual void releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer) = 0;

    virtual int imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) = 0;

    virtual ~IGraphics2D() = default;
    // Disable default constructor, copy constructor, and assignment operator

protected:
    IGraphics2D() = default;
    IGraphics2D(const IGraphics2D&) = delete;
    IGraphics2D& operator=(const IGraphics2D&) = delete;
};

} // namespace bsp_g2d

#endif // IGRAPHICS2D_HPP