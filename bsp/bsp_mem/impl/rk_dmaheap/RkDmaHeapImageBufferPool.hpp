#ifndef __BSP_MEM_RK_DMAHEAP_IMAGE_BUFFER_POOL_HPP__
#define __BSP_MEM_RK_DMAHEAP_IMAGE_BUFFER_POOL_HPP__

#include <bsp_mem/impl/ImageBufferPoolBase.hpp>

namespace bsp_mem
{
namespace impl
{

/**
 * RK3588 dma-heap 后端。按 usage 选 heap：
 * - 含 G2d / Codec / Display：system-uncached-dma32（RGA2 只能访问 4GB 以下，
 *   uncached 让 CPU 与硬件天然一致），失败退 cma-uncached
 * - 其余（CpuRead/CpuWrite/Dnn）：system（带缓存，OpenCV 读写更快）
 *
 * 带缓存的槽位在 CPU 与硬件交替访问时需要 syncBuffer（DMA_BUF_IOCTL_SYNC）；
 * uncached 槽位的 syncBuffer 是空操作。
 */
class RkDmaHeapImageBufferPool final : public ImageBufferPoolBase
{
public:
    explicit RkDmaHeapImageBufferPool(const PoolConfig& config) : ImageBufferPoolBase(config) {}

    int syncBuffer(bsp_perf::bsp_image::ImageBuffer& buffer, SyncDirection direction) override;
    std::string getBackendName() const override { return "rk_dmaheap"; }

protected:
    std::shared_ptr<Slot> allocateSlot(const AcquireRequest& request) override;
};

} // namespace impl
} // namespace bsp_mem

#endif // __BSP_MEM_RK_DMAHEAP_IMAGE_BUFFER_POOL_HPP__
