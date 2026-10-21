#ifndef __BSP_MEM_PLANE_LAYOUT_HPP__
#define __BSP_MEM_PLANE_LAYOUT_HPP__

#include <bsp_image/ImageFormat.hpp>
#include <bsp_image/ImageTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bsp_mem
{
namespace impl
{

/// 一块连续内存内的多 plane 布局（packed / 420 semi-planar / 420 planar）
struct ContiguousPlaneLayout
{
    uint32_t planeCount{0};
    std::array<size_t, 4> offsets{};
    std::array<size_t, 4> sizes{};
    std::array<uint32_t, 4> rowStrides{};
    size_t totalSize{0};
};

inline bool isSemiPlanar420(const std::string& format)
{
    return format == "YUV420SP" || format == "YCbCr_420_SP" || format == "YCrCb_420_SP";
}

inline bool isPlanar420(const std::string& format)
{
    return format == "YUV420P" || format == "YCbCr_420_P" || format == "YCrCb_420_P";
}

/// desc.widthStride / heightStride 必须已填好（对齐后的值）
inline bool computeContiguousLayout(const bsp_perf::bsp_image::ImageDesc& desc,
                                    ContiguousPlaneLayout& layout)
{
    const size_t ws = desc.widthStride;
    const size_t hs = desc.heightStride;
    if (ws == 0 || hs == 0) {
        return false;
    }

    if (isSemiPlanar420(desc.format)) {
        const size_t ySize = ws * hs;
        const size_t uvSize = ws * (hs / 2);
        layout.planeCount = 2;
        layout.offsets = {0, ySize, 0, 0};
        layout.sizes = {ySize, uvSize, 0, 0};
        layout.rowStrides = {static_cast<uint32_t>(ws), static_cast<uint32_t>(ws), 0, 0};
        layout.totalSize = ySize + uvSize;
        return true;
    }

    if (isPlanar420(desc.format)) {
        const size_t ySize = ws * hs;
        const size_t cStride = ws / 2;
        const size_t cSize = cStride * (hs / 2);
        layout.planeCount = 3;
        layout.offsets = {0, ySize, ySize + cSize, 0};
        layout.sizes = {ySize, cSize, cSize, 0};
        layout.rowStrides = {static_cast<uint32_t>(ws), static_cast<uint32_t>(cStride),
                             static_cast<uint32_t>(cStride), 0};
        layout.totalSize = ySize + 2 * cSize;
        return true;
    }

    float bpp = 0.0F;
    try {
        bpp = bsp_perf::bsp_image::bytesPerPixel(desc.format);
    } catch (...) {
        return false;  // 未登记的格式
    }
    const uint32_t bppInt = static_cast<uint32_t>(bpp);
    if (bppInt == 0 || bpp != static_cast<float>(bppInt)) {
        return false;  // 只支持整数 bpp 的 packed 格式
    }
    const uint32_t rowStride = static_cast<uint32_t>(ws) * bppInt;
    layout.planeCount = 1;
    layout.offsets = {0, 0, 0, 0};
    layout.sizes = {static_cast<size_t>(rowStride) * hs, 0, 0, 0};
    layout.rowStrides = {rowStride, 0, 0, 0};
    layout.totalSize = layout.sizes[0];
    return true;
}

inline void fillViewPlanes(bsp_perf::bsp_image::ImageView& view,
                           const ContiguousPlaneLayout& layout, uint8_t* base, int fd)
{
    view.planeCount = layout.planeCount;
    for (uint32_t i = 0; i < layout.planeCount; ++i) {
        view.planes[i].data = base + layout.offsets[i];
        view.planes[i].size = layout.sizes[i];
        view.planes[i].offset = layout.offsets[i];
        view.planes[i].rowStride = layout.rowStrides[i];
        view.planes[i].fd = fd;
    }
}

} // namespace impl
} // namespace bsp_mem

#endif // __BSP_MEM_PLANE_LAYOUT_HPP__
