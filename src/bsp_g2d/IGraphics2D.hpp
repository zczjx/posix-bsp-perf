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
     * The possible values are:
     * - "rkrga"
     * @return std::unique_ptr<IGraphics2D> A unique pointer to the created IGraphics2D instance.
     */
    static std::unique_ptr<IGraphics2D> create(const std::string& g2dPlatform);

    /**
     * @param raw_buffer raw buffer pointer
     * @param rawBufferSize The size of the buffer.
     * @param fd The file descriptor of the buffer.
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
     * - "YUV420SP"
     * - "YCbCr_420_P"
     * - "YUV420P"
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
     */
    struct G2DBufferParams
    {
        void* virtual_addr{nullptr};
        void* physical_addr{nullptr};
        int fd{-1};
        std::any handle;
        size_t rawBufferSize;
        size_t width;
        size_t height;
        size_t width_stride;
        size_t height_stride;
        std::string format;
    };

    struct G2DBuffer
    {
        /// default is rkrga, will add more types later
        std::string g2dPlatform{"rkrga"};
        /**
         * @brief Type of G2D buffer Mapping Type.
         *
         * This string indicates the type of G2D buffer being used.
         * The possible values for g2dBufferType are:
         * - "fd": File descriptor
         * - "virtualaddr": Virtual address
         * - "physicaladdr": Physical address
         * - "handle": Handle
         */
        std::string g2dBufferMapType{"virtualaddr"};
        /// any should be a pointer to buffer type
        std::any g2dBufferHandle;
        uint8_t* rawBuffer{nullptr};
        size_t rawBufferSize{0};
    };

    /**
     * @brief Creates a G2D buffer.
     *
     * This function creates a G2D buffer with the specified virtual address, width, height, and format.
     * The buffer is created based on the specified G2D buffer mapping type.
     *
     * @param g2dBufferMapType The type of G2D buffer mapping to be used.
     * The possible values for g2dBufferMapType are:
     * - "fd": File descriptor
     * - "virtualaddr": Virtual address
     * - "physicaladdr": Physical address
     * - "handle": Handle
     * @param params The parameters for the G2D buffer.
     *
     * @return std::shared_ptr<G2DBuffer> A shared pointer to the created G2D buffer.
     */
    virtual std::shared_ptr<G2DBuffer> createG2DBuffer(const std::string& g2dBufferMapType, G2DBufferParams& params) = 0;

    virtual void releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer) = 0;

    virtual int imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) = 0;

    virtual int imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) = 0;

    virtual ~IGraphics2D() = default;
    // Disable default constructor, copy constructor, and assignment operator

protected:
    IGraphics2D() = default;
    IGraphics2D(const IGraphics2D&) = delete;
    IGraphics2D& operator=(const IGraphics2D&) = delete;
};

} // namespace bsp_g2d

#endif // IGRAPHICS2D_HPP