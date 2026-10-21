#ifndef __BSP_IMAGE_TYPES_HPP__
#define __BSP_IMAGE_TYPES_HPP__

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bsp_perf
{
namespace bsp_image
{
enum class ImageMemoryType
{
    Host,
    DmaBuf,
    SharedMemorySlot,
    NativeHandle
};

enum class ImageAccess
{
    ReadOnly,
    WriteOnly,
    ReadWrite
};

/**
 * 缓冲的预期使用者（参考 Android BufferUsage）。分配器据此选 heap、对齐和缓存策略。
 * 位掩码，可用 | 组合。
 */
enum class ImageUsage : uint32_t
{
    None     = 0,
    CpuRead  = 1u << 0,
    CpuWrite = 1u << 1,
    G2d      = 1u << 2, // RGA / VIC
    Codec    = 1u << 3, // MPP / NVDEC / NVENC
    Dnn      = 1u << 4, // RKNN / TensorRT 输入
    Display  = 1u << 5  // VOP / DRM scanout
};

constexpr ImageUsage operator|(ImageUsage a, ImageUsage b)
{
    return static_cast<ImageUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr bool hasUsage(ImageUsage set, ImageUsage flag)
{
    return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

/// 需要 IP 做 DMA 的用途
constexpr bool needsDeviceDma(ImageUsage usage)
{
    return hasUsage(usage, ImageUsage::G2d | ImageUsage::Codec | ImageUsage::Display);
}

struct ImageSize
{
    uint32_t width{0};
    uint32_t height{0};

    bool empty() const { return width == 0 || height == 0; }
};

struct ImageDesc
{
    uint32_t width{0};
    uint32_t height{0};
    uint32_t widthStride{0};
    uint32_t heightStride{0};
    std::string format{};
    size_t dataSize{0};

    bool empty() const { return width == 0 || height == 0 || format.empty(); }
};

struct ImagePlane
{
    uint8_t* data{nullptr};
    size_t size{0};
    size_t offset{0};
    uint32_t rowStride{0};
    int fd{-1};
};

struct ImageView
{
    ImageDesc desc{};
    ImageMemoryType memoryType{ImageMemoryType::Host};
    ImageAccess access{ImageAccess::ReadWrite};
    std::array<ImagePlane, 4> planes{};
    uint32_t planeCount{1};

    uint8_t* data(size_t plane = 0) const
    {
        return plane < planes.size() ? planes[plane].data : nullptr;
    }

    bool empty() const
    {
        return desc.empty() || planeCount == 0 || data() == nullptr;
    }
};

inline ImageView makeHostImageView(uint8_t* data, const ImageDesc& desc,
                                   uint32_t rowStride = 0,
                                   ImageAccess access = ImageAccess::ReadWrite)
{
    ImageView view{};
    view.desc = desc;
    if (view.desc.widthStride == 0) {
        view.desc.widthStride = view.desc.width;
    }
    if (view.desc.heightStride == 0) {
        view.desc.heightStride = view.desc.height;
    }
    view.memoryType = ImageMemoryType::Host;
    view.access = access;
    view.planeCount = 1;
    view.planes[0].data = data;
    view.planes[0].size = view.desc.dataSize;
    view.planes[0].rowStride = rowStride > 0 ? rowStride : view.desc.widthStride;
    view.planes[0].fd = -1;
    return view;
}

} // namespace bsp_image
} // namespace bsp_perf

#endif // __BSP_IMAGE_TYPES_HPP__
