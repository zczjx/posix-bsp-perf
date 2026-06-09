#ifndef __BSP_IMAGE_BUFFER_HPP__
#define __BSP_IMAGE_BUFFER_HPP__

#include "ImageFormat.hpp"
#include <any>
#include <cstring>
#include <functional>
#include <memory>

namespace bsp_perf
{
namespace bsp_image
{

struct ImageBuffer
{
    ImageView view{};
    std::shared_ptr<void> owner{};
    std::any nativeHandle{};
    std::function<void()> release{};

    ~ImageBuffer()
    {
        if (release) {
            release();
        }
    }

    ImageBuffer() = default;
    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;
    ImageBuffer(ImageBuffer&&) = default;
    ImageBuffer& operator=(ImageBuffer&&) = default;
};

inline std::shared_ptr<ImageBuffer> makeHostImageBuffer(const ImageDesc& desc)
{
    auto buffer = std::make_shared<ImageBuffer>();
    buffer->view.desc = desc;
    if (buffer->view.desc.dataSize == 0) {
        buffer->view.desc.dataSize = imageDataSize(desc);
    }

    auto data = std::shared_ptr<uint8_t>(
        new uint8_t[buffer->view.desc.dataSize],
        std::default_delete<uint8_t[]>());
    std::memset(data.get(), 0, buffer->view.desc.dataSize);

    buffer->owner = data;
    buffer->view.owner = data;
    buffer->view.memoryType = ImageMemoryType::Host;
    buffer->view.planeCount = 1;
    buffer->view.planes[0].data = data.get();
    buffer->view.planes[0].size = buffer->view.desc.dataSize;
    buffer->view.planes[0].rowStride = buffer->view.desc.widthStride > 0 ? buffer->view.desc.widthStride : buffer->view.desc.width;
    buffer->view.planes[0].fd = -1;
    return buffer;
}

} // namespace bsp_image
} // namespace bsp_perf

#endif // __BSP_IMAGE_BUFFER_HPP__
