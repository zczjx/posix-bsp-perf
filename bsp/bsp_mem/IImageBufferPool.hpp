#ifndef __BSP_MEM_IIMAGE_BUFFER_POOL_HPP__
#define __BSP_MEM_IIMAGE_BUFFER_POOL_HPP__

// 设计参考 Android Gralloc 的 usage 驱动分配：分配时声明使用者（ImageUsage），
// 后端据此选 heap / 对齐 / 缓存策略。不实现 Gralloc HAL 接口本身。

#include <bsp_image/ImageBuffer.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace bsp_mem
{

enum class SyncDirection
{
    CpuToDevice,  // CPU 写完，交给硬件之前
    DeviceToCpu,  // 硬件写完，CPU 读之前
    Bidirectional
};

struct PoolConfig
{
    size_t capacity{8};        // 池内总槽位数
    uint32_t alignPixels{16};  // 宽 stride 像素对齐
};

struct AcquireRequest
{
    bsp_perf::bsp_image::ImageDesc desc{};  // 只需填 width / height / format
    bsp_perf::bsp_image::ImageUsage usage{bsp_perf::bsp_image::ImageUsage::None};
};

/**
 * @brief 硬件加速器可访问图像内存的分配池
 *
 * - acquire() 返回 shared_ptr<ImageBuffer>，最后一个引用析构时槽位自动还池。
 * - 池满且无可回收的空闲槽位时返回 nullptr，调用方丢帧。
 * - 返回的缓冲内容未定义；首次访问方向需要调用方自己 syncBuffer()。
 * - planes[i].fd 交给硬件 IP，planes[i].data 给 CPU。
 */
class IImageBufferPool
{
public:
    /// backend: "auto" | "host" | "rk_dmaheap" | "nv_surface"；不可用返回 nullptr
    static std::unique_ptr<IImageBufferPool> create(
        const std::string& backend, const PoolConfig& config = {});

    virtual ~IImageBufferPool() = default;

    virtual std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> acquire(
        const AcquireRequest& request) = 0;

    virtual int syncBuffer(bsp_perf::bsp_image::ImageBuffer& buffer,
                           SyncDirection direction) = 0;

    virtual std::string getBackendName() const = 0;
    virtual size_t capacity() const = 0;
};

} // namespace bsp_mem

#endif // __BSP_MEM_IIMAGE_BUFFER_POOL_HPP__
