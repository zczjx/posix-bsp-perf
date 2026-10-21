#include "RkDmaHeapImageBufferPool.hpp"

#include <bsp_mem/impl/PlaneLayout.hpp>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bsp_mem
{
namespace impl
{

using bsp_perf::bsp_image::ImageUsage;

namespace
{

/// 存进 Slot::platformHandle，syncBuffer 据此判断是否要刷 cache
struct RkSlotInfo
{
    int fd{-1};
    bool cached{false};
};

/// 按 usage 给出依次尝试的 heap；cached 标记该 heap 的 CPU 映射是否带缓存
struct HeapChoice
{
    const char* path;
    bool cached;
};

std::vector<HeapChoice> heapCandidates(ImageUsage usage)
{
    if (bsp_perf::bsp_image::needsDeviceDma(usage)) {
        return {{"/dev/dma_heap/system-uncached-dma32", false},
                {"/dev/dma_heap/cma-uncached", false}};
    }
    // 纯 CPU / DNN 用途：带缓存 heap，失败时退回 uncached
    return {{"/dev/dma_heap/system", true},
            {"/dev/dma_heap/system-uncached-dma32", false}};
}

int allocFromHeap(const HeapChoice& heap, size_t size)
{
    const int heapFd = open(heap.path, O_RDWR | O_CLOEXEC);
    if (heapFd < 0) {
        return -1;
    }
    dma_heap_allocation_data allocation{};
    allocation.len = size;
    allocation.fd_flags = O_RDWR | O_CLOEXEC;
    const int ret = ioctl(heapFd, DMA_HEAP_IOCTL_ALLOC, &allocation);
    close(heapFd);
    return ret == 0 ? static_cast<int>(allocation.fd) : -1;
}

} // namespace

std::shared_ptr<ImageBufferPoolBase::Slot> RkDmaHeapImageBufferPool::allocateSlot(
    const AcquireRequest& request)
{
    bsp_perf::bsp_image::ImageDesc desc = request.desc;
    desc.widthStride = alignWidthStride(desc);
    desc.heightStride = desc.heightStride > 0 ? desc.heightStride : desc.height;

    ContiguousPlaneLayout layout{};
    if (!computeContiguousLayout(desc, layout)) {
        std::cerr << "RkDmaHeapImageBufferPool: unsupported format " << desc.format << std::endl;
        return nullptr;
    }
    desc.dataSize = layout.totalSize;

    int dmaFd = -1;
    bool cached = false;
    for (const HeapChoice& heap : heapCandidates(request.usage)) {
        dmaFd = allocFromHeap(heap, layout.totalSize);
        if (dmaFd >= 0) {
            cached = heap.cached;
            break;
        }
    }
    if (dmaFd < 0) {
        std::cerr << "RkDmaHeapImageBufferPool: DMA_HEAP_IOCTL_ALLOC failed ("
                  << layout.totalSize << " bytes): " << strerror(errno) << std::endl;
        return nullptr;
    }

    void* data = mmap(nullptr, layout.totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, dmaFd, 0);
    if (data == MAP_FAILED) {
        std::cerr << "RkDmaHeapImageBufferPool: mmap failed: " << strerror(errno) << std::endl;
        close(dmaFd);
        return nullptr;
    }
    std::memset(data, 0, layout.totalSize);

    const size_t size = layout.totalSize;
    auto memory = std::shared_ptr<void>(data, [size, dmaFd](void* ptr) {
        munmap(ptr, size);
        close(dmaFd);
    });

    auto slot = std::make_shared<Slot>();
    slot->view.desc = desc;
    slot->view.memoryType = bsp_perf::bsp_image::ImageMemoryType::DmaBuf;
    slot->view.access = bsp_perf::bsp_image::ImageAccess::ReadWrite;
    fillViewPlanes(slot->view, layout, static_cast<uint8_t*>(data), dmaFd);
    slot->memory = std::move(memory);
    slot->platformHandle = RkSlotInfo{dmaFd, cached};
    return slot;
}

int RkDmaHeapImageBufferPool::syncBuffer(bsp_perf::bsp_image::ImageBuffer& buffer,
                                         SyncDirection direction)
{
    const RkSlotInfo* info = std::any_cast<RkSlotInfo>(&buffer.nativeHandle);
    if (!info) {
        std::cerr << "RkDmaHeapImageBufferPool: buffer was not acquired from this pool"
                  << std::endl;
        return -1;
    }
    if (!info->cached) {
        return 0;  // uncached heap：无需 cache 维护
    }

    // CPU 写完交给硬件：END 时 flush；硬件写完 CPU 读：START 时 invalidate。
    // 带缓存槽位上两个方向各发一对 START/END，保证两个方向都被覆盖。
    auto doSync = [&](uint64_t flags) {
        dma_buf_sync sync{};
        sync.flags = flags;
        return ioctl(info->fd, DMA_BUF_IOCTL_SYNC, &sync);
    };

    int ret = 0;
    if (direction == SyncDirection::CpuToDevice || direction == SyncDirection::Bidirectional) {
        ret |= doSync(DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE);
        ret |= doSync(DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE);
    }
    if (direction == SyncDirection::DeviceToCpu || direction == SyncDirection::Bidirectional) {
        ret |= doSync(DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
        ret |= doSync(DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
    }
    return ret == 0 ? 0 : -1;
}

} // namespace impl
} // namespace bsp_mem
