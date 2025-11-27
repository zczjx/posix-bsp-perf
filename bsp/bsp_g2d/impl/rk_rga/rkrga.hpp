#ifndef __RK_RGA_HPP__
#define __RK_RGA_HPP__
#include <bsp_g2d/IGraphics2D.hpp>
#include <rga/im2d.h>
#include <rga/rga.h>
#include <rga/RgaUtils.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace bsp_g2d
{
class rgaPixelFormat
{
public:
    static rgaPixelFormat& getInstance()
    {
        static rgaPixelFormat instance;
        return instance;
    }

    RgaSURF_FORMAT strToRgaPixFormat(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return strToRgaPixFormatMap.at(str);
    }

private:
    rgaPixelFormat() = default;
    ~rgaPixelFormat() = default;
    rgaPixelFormat(const rgaPixelFormat&) = delete;
    rgaPixelFormat& operator=(const rgaPixelFormat&) = delete;

private:
    std::mutex m_mutex;
    const std::unordered_map<std::string, RgaSURF_FORMAT> strToRgaPixFormatMap {
        {"RGBA8888", RK_FORMAT_RGBA_8888},
        {"RGBX8888", RK_FORMAT_RGBX_8888},
        {"RGB888", RK_FORMAT_RGB_888},
        {"BGRA8888", RK_FORMAT_BGRA_8888},
        {"RGB565", RK_FORMAT_RGB_565},
        {"RGBA5551", RK_FORMAT_RGBA_5551},
        {"RGBA4444", RK_FORMAT_RGBA_4444},
        {"BGR888", RK_FORMAT_BGR_888},
        {"YCbCr_422_SP", RK_FORMAT_YCbCr_422_SP},
        {"YCbCr_422_P", RK_FORMAT_YCbCr_422_P},
        {"YCbCr_420_SP", RK_FORMAT_YCbCr_420_SP},
        {"YUV420SP", RK_FORMAT_YCbCr_420_SP},
        {"YCbCr_420_P", RK_FORMAT_YCbCr_420_P},
        {"YUV420P", RK_FORMAT_YCbCr_420_P},
        {"YCrCb_422_SP", RK_FORMAT_YCrCb_422_SP},
        {"YCrCb_422_P", RK_FORMAT_YCrCb_422_P},
        {"YCrCb_420_SP", RK_FORMAT_YCrCb_420_SP},
        {"YCrCb_420_P", RK_FORMAT_YCrCb_420_P},
        {"BPP1", RK_FORMAT_BPP1},
        {"BPP2", RK_FORMAT_BPP2},
        {"BPP4", RK_FORMAT_BPP4},
        {"BPP8", RK_FORMAT_BPP8},
        {"Y4", RK_FORMAT_Y4},
        {"YCbCr_400", RK_FORMAT_YCbCr_400},
        {"BGRX8888", RK_FORMAT_BGRX_8888},
        {"YVYU_422", RK_FORMAT_YVYU_422},
        {"YVYU_420", RK_FORMAT_YVYU_420},
        {"VYUY_422", RK_FORMAT_VYUY_422},
        {"VYUY_420", RK_FORMAT_VYUY_420},
        {"YUYV_422", RK_FORMAT_YUYV_422},
        {"YUYV_420", RK_FORMAT_YUYV_420},
        {"UYVY_422", RK_FORMAT_UYVY_422},
        {"UYVY_420", RK_FORMAT_UYVY_420},
        {"YCbCr_420_SP_10B", RK_FORMAT_YCbCr_420_SP_10B},
        {"YCrCb_420_SP_10B", RK_FORMAT_YCrCb_420_SP_10B},
        {"YCbCr_422_SP_10B", RK_FORMAT_YCbCr_422_SP_10B},
        {"YCrCb_422_SP_10B", RK_FORMAT_YCrCb_422_SP_10B},
        {"BGR565", RK_FORMAT_BGR_565},
        {"BGRA5551", RK_FORMAT_BGRA_5551},
        {"BGRA4444", RK_FORMAT_BGRA_4444},
        {"ARGB8888", RK_FORMAT_ARGB_8888},
        {"XRGB8888", RK_FORMAT_XRGB_8888},
        {"ARGB5551", RK_FORMAT_ARGB_5551},
        {"ARGB4444", RK_FORMAT_ARGB_4444},
        {"ABGR8888", RK_FORMAT_ABGR_8888},
        {"XBGR8888", RK_FORMAT_XBGR_8888},
        {"ABGR5551", RK_FORMAT_ABGR_5551},
        {"ABGR4444", RK_FORMAT_ABGR_4444},
        {"RGBA2BPP", RK_FORMAT_RGBA2BPP},
        {"A8", RK_FORMAT_A8},
        {"YCbCr_444_SP", RK_FORMAT_YCbCr_444_SP},
        {"YCrCb_444_SP", RK_FORMAT_YCrCb_444_SP},
        {"Y8", RK_FORMAT_Y8},
        {"UNKNOWN", RK_FORMAT_UNKNOWN}
    };
};

class rkrga : public IGraphics2D
{
public:
    rkrga() = default;
    rkrga(const rkrga&) = delete;
    rkrga& operator=(const rkrga&) = delete;
    rkrga(rkrga&&) = delete;
    rkrga& operator=(rkrga&&) = delete;
    ~rkrga() = default;

    // ========== New Interface ==========
    
    std::shared_ptr<G2DBuffer> createBuffer(
        BufferType type,
        const G2DBufferParams& params) override;

    void releaseBuffer(std::shared_ptr<G2DBuffer> buffer) override;

    int syncBuffer(
        std::shared_ptr<G2DBuffer> buffer,
        SyncDirection direction) override;

    void* mapBuffer(
        std::shared_ptr<G2DBuffer> buffer,
        const std::string& access_mode = "readwrite") override;

    void unmapBuffer(std::shared_ptr<G2DBuffer> buffer) override;

    bool queryCapability(const std::string& capability) const override;

    std::string getPlatformName() const override;

    // ========== Legacy Interface (for backward compatibility) ==========

    std::shared_ptr<IGraphics2D::G2DBuffer> createG2DBuffer(
        const std::string& g2dBufferMapType, 
        G2DBufferParams& params) override;

    void releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer) override;

    // ========== Image Operations ==========

    int imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

    int imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

    int imageDrawRectangle(std::shared_ptr<G2DBuffer> dst, ImageRect& rect, uint32_t color, int thickness) override;

    int imageCvtColor(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst,
                    const std::string& src_format, const std::string& dst_format) override;
};

} // namespace bsp_g2d
#endif // RK_RGA_HPP
