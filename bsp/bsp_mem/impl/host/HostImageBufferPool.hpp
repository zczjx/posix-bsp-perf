#ifndef __BSP_MEM_HOST_IMAGE_BUFFER_POOL_HPP__
#define __BSP_MEM_HOST_IMAGE_BUFFER_POOL_HPP__

#include <bsp_mem/impl/ImageBufferPoolBase.hpp>

namespace bsp_mem
{
namespace impl
{

/// 普通 CPU 内存后端：x86 调试和纯 CPU 流水线用。fd 为 -1，不能交给硬件 IP。
class HostImageBufferPool final : public ImageBufferPoolBase
{
public:
    explicit HostImageBufferPool(const PoolConfig& config) : ImageBufferPoolBase(config) {}

    int syncBuffer(bsp_perf::bsp_image::ImageBuffer&, SyncDirection) override { return 0; }
    std::string getBackendName() const override { return "host"; }

protected:
    std::shared_ptr<Slot> allocateSlot(const AcquireRequest& request) override;
};

} // namespace impl
} // namespace bsp_mem

#endif // __BSP_MEM_HOST_IMAGE_BUFFER_POOL_HPP__
