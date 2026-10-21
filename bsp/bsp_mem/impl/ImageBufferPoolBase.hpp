#ifndef __BSP_MEM_IMAGE_BUFFER_POOL_BASE_HPP__
#define __BSP_MEM_IMAGE_BUFFER_POOL_BASE_HPP__

#include <bsp_mem/IImageBufferPool.hpp>

#include <any>
#include <memory>
#include <mutex>
#include <vector>

namespace bsp_mem
{
namespace impl
{

/// 平台无关骨架：槽位匹配、容量控制、空闲回收、自动还槽。
/// 平台子类只实现 allocateSlot()。
class ImageBufferPoolBase : public IImageBufferPool
{
public:
    explicit ImageBufferPoolBase(const PoolConfig& config);
    ~ImageBufferPoolBase() override = default;

    ImageBufferPoolBase(const ImageBufferPoolBase&) = delete;
    ImageBufferPoolBase& operator=(const ImageBufferPoolBase&) = delete;

    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> acquire(
        const AcquireRequest& request) override;
    size_t capacity() const override;

protected:
    struct Slot
    {
        bsp_perf::bsp_image::ImageView view{};
        bsp_perf::bsp_image::ImageUsage usage{bsp_perf::bsp_image::ImageUsage::None};
        std::shared_ptr<void> memory{};  // 内存所有权，deleter 做平台释放
        std::any platformHandle{};       // 如 NvBufSurface*
        bool inUse{false};
    };

    /// 平台分配新槽位，失败返回 nullptr。调用时不持池锁。
    virtual std::shared_ptr<Slot> allocateSlot(const AcquireRequest& request) = 0;

    /// 宽 stride 按配置对齐，420 格式保证偶数
    uint32_t alignWidthStride(const bsp_perf::bsp_image::ImageDesc& desc) const;

private:
    struct PoolState
    {
        explicit PoolState(const PoolConfig& cfg) : config(cfg) {}
        std::mutex mutex;
        std::vector<std::shared_ptr<Slot>> slots;  // 空指针 = 已预留、分配中
        PoolConfig config;
    };

    std::shared_ptr<PoolState> m_state;
};

} // namespace impl
} // namespace bsp_mem

#endif // __BSP_MEM_IMAGE_BUFFER_POOL_BASE_HPP__
