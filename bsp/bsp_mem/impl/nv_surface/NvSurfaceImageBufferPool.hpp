#ifndef __BSP_MEM_NV_SURFACE_IMAGE_BUFFER_POOL_HPP__
#define __BSP_MEM_NV_SURFACE_IMAGE_BUFFER_POOL_HPP__

#include <bsp_mem/impl/ImageBufferPoolBase.hpp>

namespace bsp_mem
{
namespace impl
{

/**
 * Jetson NvBufSurface 后端：NvBufSurfaceAllocate 分配 NVBUF_MEM_SURFACE_ARRAY，
 * 分配时把各 plane 映射给 CPU。nativeHandle 里是 NvBufSurface*，planes[i].fd 是
 * dma-buf fd。VIC 不支持 24-bit RGB/BGR，请用 RGBA8888 / BGRA8888。
 * CPU 与硬件交替访问前后必须 syncBuffer()。
 */
class NvSurfaceImageBufferPool final : public ImageBufferPoolBase
{
public:
    explicit NvSurfaceImageBufferPool(const PoolConfig& config) : ImageBufferPoolBase(config) {}

    int syncBuffer(bsp_perf::bsp_image::ImageBuffer& buffer, SyncDirection direction) override;
    std::string getBackendName() const override { return "nv_surface"; }

protected:
    std::shared_ptr<Slot> allocateSlot(const AcquireRequest& request) override;
};

} // namespace impl
} // namespace bsp_mem

#endif // __BSP_MEM_NV_SURFACE_IMAGE_BUFFER_POOL_HPP__
