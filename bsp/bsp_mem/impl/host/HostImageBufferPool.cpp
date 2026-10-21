#include "HostImageBufferPool.hpp"

#include <bsp_mem/impl/PlaneLayout.hpp>

#include <iostream>

namespace bsp_mem
{
namespace impl
{

std::shared_ptr<ImageBufferPoolBase::Slot> HostImageBufferPool::allocateSlot(
    const AcquireRequest& request)
{
    bsp_perf::bsp_image::ImageDesc desc = request.desc;
    desc.widthStride = alignWidthStride(desc);
    desc.heightStride = desc.heightStride > 0 ? desc.heightStride : desc.height;

    ContiguousPlaneLayout layout{};
    if (!computeContiguousLayout(desc, layout)) {
        std::cerr << "HostImageBufferPool: unsupported format " << desc.format << std::endl;
        return nullptr;
    }
    desc.dataSize = layout.totalSize;

    auto data = std::shared_ptr<uint8_t>(new uint8_t[layout.totalSize](),
                                         std::default_delete<uint8_t[]>());

    auto slot = std::make_shared<Slot>();
    slot->view.desc = desc;
    slot->view.memoryType = bsp_perf::bsp_image::ImageMemoryType::Host;
    slot->view.access = bsp_perf::bsp_image::ImageAccess::ReadWrite;
    fillViewPlanes(slot->view, layout, data.get(), -1);
    slot->memory = std::move(data);
    return slot;
}

} // namespace impl
} // namespace bsp_mem
