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
