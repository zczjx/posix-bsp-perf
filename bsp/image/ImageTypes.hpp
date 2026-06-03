#ifndef __BSP_IMAGE_TYPES_HPP__
#define __BSP_IMAGE_TYPES_HPP__

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace bsp_perf
{
namespace image
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
    std::shared_ptr<void> owner{};

    uint8_t* data(size_t plane = 0) const
    {
        return plane < planes.size() ? planes[plane].data : nullptr;
    }

    bool empty() const
    {
        return desc.empty() || planeCount == 0 || data() == nullptr;
    }
};

} // namespace image

namespace shared
{
using image::ImageAccess;
using image::ImageDesc;
using image::ImageMemoryType;
using image::ImagePlane;
using image::ImageSize;
using image::ImageView;
} // namespace shared
} // namespace bsp_perf

#endif // __BSP_IMAGE_TYPES_HPP__
