#ifndef __G2D_BUFFER_INTERNAL_HPP__
#define __G2D_BUFFER_INTERNAL_HPP__

#include <bsp_g2d/IGraphics2D.hpp>
#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace bsp_g2d
{
namespace impl
{

struct G2DBufferInternal
{
    std::string g2dPlatform{};
    IGraphics2D::BufferType bufferType{IGraphics2D::BufferType::Mapped};
    std::any g2dBufferHandle{};
    uint8_t* hostPtr{nullptr};
    size_t bufferSize{0};
    void* platformData{nullptr};
};

inline std::shared_ptr<G2DBufferInternal> getG2DBufferInternal(
    const std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>& buffer)
{
    if (!buffer || !buffer->nativeHandle.has_value()) {
        return nullptr;
    }
    return std::any_cast<std::shared_ptr<G2DBufferInternal>>(buffer->nativeHandle);
}

inline const bsp_perf::bsp_image::ImageView& imageView(const bsp_perf::bsp_image::ImageBuffer& buffer)
{
    return buffer.view;
}

} // namespace impl
} // namespace bsp_g2d

#endif // __G2D_BUFFER_INTERNAL_HPP__
