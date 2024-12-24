#ifndef __RK_RGA_HPP__
#define __RK_RGA_HPP__
#include <bsp_g2d/IGraphics2D.hpp>
#include <rga/im2d.h>
#include <rga/rga.h>
#include <rga/RgaUtils.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace bsp_g2d
{
struct RGAParams
{
    const std::unordered_map<std::string, RgaSURF_FORMAT> m_pix_format_map{
        {"RGBA_8888", RK_FORMAT_RGBA_8888},
        {"RGBX_8888", RK_FORMAT_RGBX_8888},
        {"RGB_888", RK_FORMAT_RGB_888},
        {"BGRA_8888", RK_FORMAT_BGRA_8888},
        {"RGB_565", RK_FORMAT_RGB_565},
        {"RGBA_5551", RK_FORMAT_RGBA_5551},
        {"RGBA_4444", RK_FORMAT_RGBA_4444},
        {"BGR_888", RK_FORMAT_BGR_888},
        {"YCbCr_422_SP", RK_FORMAT_YCbCr_422_SP},
        {"YCbCr_422_P", RK_FORMAT_YCbCr_422_P},
        {"YCbCr_420_SP", RK_FORMAT_YCbCr_420_SP},
        {"YCbCr_420_P", RK_FORMAT_YCbCr_420_P},
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
        {"BGRX_8888", RK_FORMAT_BGRX_8888},
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
        {"BGR_565", RK_FORMAT_BGR_565},
        {"BGRA_5551", RK_FORMAT_BGRA_5551},
        {"BGRA_4444", RK_FORMAT_BGRA_4444},
        {"ARGB_8888", RK_FORMAT_ARGB_8888},
        {"XRGB_8888", RK_FORMAT_XRGB_8888},
        {"ARGB_5551", RK_FORMAT_ARGB_5551},
        {"ARGB_4444", RK_FORMAT_ARGB_4444},
        {"ABGR_8888", RK_FORMAT_ABGR_8888},
        {"XBGR_8888", RK_FORMAT_XBGR_8888},
        {"ABGR_5551", RK_FORMAT_ABGR_5551},
        {"ABGR_4444", RK_FORMAT_ABGR_4444},
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

    std::shared_ptr<IGraphics2D::G2DBuffer> createG2DBuffer(const std::string& g2dBufferMapType, G2DBufferParams& params) override;


    void releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer) override;

    int imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

    int imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst) override;

private:
    RGAParams m_rgaParams{};
};

} // namespace bsp_g2d
#endif // RK_RGA_HPP