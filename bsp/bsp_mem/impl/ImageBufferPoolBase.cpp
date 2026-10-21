#include "ImageBufferPoolBase.hpp"

namespace bsp_mem
{
namespace impl
{

using bsp_perf::bsp_image::ImageBuffer;
using bsp_perf::bsp_image::ImageDesc;

namespace
{

bool isChroma420(const std::string& format)
{
    return format == "YUV420SP" || format == "YCbCr_420_SP" || format == "YCrCb_420_SP" ||
           format == "YUV420P" || format == "YCbCr_420_P" || format == "YCrCb_420_P";
}

} // namespace

ImageBufferPoolBase::ImageBufferPoolBase(const PoolConfig& config)
    : m_state(std::make_shared<PoolState>(config))
{
    if (m_state->config.capacity == 0) {
        m_state->config.capacity = 1;
    }
    if (m_state->config.alignPixels == 0) {
        m_state->config.alignPixels = 1;
    }
}

size_t ImageBufferPoolBase::capacity() const
{
    return m_state->config.capacity;
}

uint32_t ImageBufferPoolBase::alignWidthStride(const ImageDesc& desc) const
{
    const uint32_t tight = desc.widthStride > 0 ? desc.widthStride : desc.width;
    const uint32_t align = m_state->config.alignPixels;
    uint32_t aligned = (tight + align - 1) / align * align;
    if (isChroma420(desc.format)) {
        aligned = (aligned + 1) & ~1u;
    }
    return aligned;
}

std::shared_ptr<ImageBuffer> ImageBufferPoolBase::acquire(const AcquireRequest& request)
{
    const ImageDesc& want = request.desc;
    if (want.width == 0 || want.height == 0 || want.format.empty()) {
        return nullptr;
    }

    std::shared_ptr<Slot> slot;
    size_t slotIndex = 0;
    bool reserved = false;

    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        auto& slots = m_state->slots;

        // 1) 复用：几何、格式、用途完全一致的空闲槽位
        for (size_t i = 0; i < slots.size(); ++i) {
            Slot* s = slots[i].get();
            if (s && !s->inUse && s->usage == request.usage &&
                s->view.desc.width == want.width && s->view.desc.height == want.height &&
                s->view.desc.format == want.format) {
                s->inUse = true;
                slot = slots[i];
                slotIndex = i;
                break;
            }
        }

        // 2) 没有可复用的：池未满则新增预留位；已满则回收一个不匹配的空闲槽位
        if (!slot) {
            if (slots.size() < m_state->config.capacity) {
                slots.push_back(nullptr);
                slotIndex = slots.size() - 1;
                reserved = true;
            } else {
                for (size_t i = 0; i < slots.size(); ++i) {
                    if (slots[i] && !slots[i]->inUse) {
                        slots[i].reset();  // 释放旧内存（借出者持有的 owner 不受影响）
                        slotIndex = i;
                        reserved = true;
                        break;
                    }
                }
            }
            if (!reserved) {
                return nullptr;  // 全部在用：调用方丢帧
            }
        }
    }

    // 3) 锁外分配（ioctl / mmap / NvBufSurfaceAllocate 都可能慢）
    if (reserved) {
        std::shared_ptr<Slot> created = allocateSlot(request);
        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (!created) {
            // 分配失败：撤销预留位，让出名额
            if (slotIndex + 1 == m_state->slots.size()) {
                m_state->slots.pop_back();
            }
            return nullptr;
        }
        created->usage = request.usage;
        created->inUse = true;
        m_state->slots[slotIndex] = created;
        slot = created;
    }

    auto buffer = std::make_shared<ImageBuffer>();
    buffer->view = slot->view;
    buffer->nativeHandle = slot->platformHandle;
    buffer->owner = slot->memory;

    std::weak_ptr<PoolState> weakState = m_state;
    std::weak_ptr<Slot> weakSlot = slot;
    buffer->release = [weakState, weakSlot]() {
        auto state = weakState.lock();
        auto s = weakSlot.lock();
        if (!state || !s) {
            return;  // 池或槽位已先消亡
        }
        std::lock_guard<std::mutex> guard(state->mutex);
        s->inUse = false;
    };
    return buffer;
}

} // namespace impl
} // namespace bsp_mem
