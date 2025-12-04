#ifndef __IGRAPHICS2D_HPP__
#define __IGRAPHICS2D_HPP__

#include <memory>
#include <string>
#include <any>

namespace bsp_g2d
{

class IGraphics2D
{
public:
    // ========== Factory ==========
    /**
     * @brief Factory function to create an instance of IGraphics2D.
     * @param g2dPlatform Graphics platform: "rkrga", "nvvic"
     * @return std::unique_ptr<IGraphics2D>
     */
    static std::unique_ptr<IGraphics2D> create(const std::string& g2dPlatform);

    // ========== Buffer Types ==========

    /**
     * @brief 缓冲区类型枚举
     * 
     * Hardware: 硬件加速器直接访问的内存（RGA 可访问任意内存，VIC 需要 NVMAP）
     * Mapped: 可以同时被 CPU 和硬件访问的内存（带同步）
     */
    enum class BufferType
    {
        /**
         * 硬件专用缓冲区（推荐用于中间结果）
         * - RGA: 使用 fd/handle
         * - VIC: 使用 NVBUF_MEM_SURFACE_ARRAY
         * - 特点：高性能，不能直接被 CPU 访问（需要 map）
         */
        Hardware,

        /**
         * CPU-GPU 共享缓冲区（推荐用于需要 CPU 处理的数据，如 OpenCV）
         * - RGA: 直接使用用户提供的 virtualaddr
         * - VIC: 内部管理 NVMAP + 自动同步
         * - 特点：可以被 CPU 直接访问，但需要显式同步
         */
        Mapped
    };

    /**
     * @brief 内存同步方向
     */
    enum class SyncDirection
    {
        CpuToDevice,    // CPU 修改后，同步到硬件（写入前）
        DeviceToCpu,    // 硬件修改后，同步到 CPU（读取前）
        Bidirectional   // 双向同步
    };

    /**
     * @brief 缓冲区参数
     */
    struct G2DBufferParams
    {
        // ===== 通用参数 =====
        size_t width;
        size_t height;
        size_t width_stride{0};   // 0 = 自动计算
        size_t height_stride{0};  // 0 = 自动计算
        std::string format;       // "RGBA8888", "YUV420SP", etc.

        // ===== Hardware 类型专用 =====
        int fd{-1};               // RGA: fd-based buffer
        std::any handle;          // Platform-specific handle

        // ===== Mapped 类型专用 =====
        void* host_ptr{nullptr};  // 用户提供的 CPU 可访问内存
        size_t buffer_size{0};    // host_ptr 的大小
    };

    /**
     * @brief G2D 缓冲区（不透明句柄）
     */
    struct G2DBuffer
    {
        std::string g2dPlatform;       // "rkrga" or "nvvic"
        BufferType bufferType;         // Hardware or Mapped
        std::any g2dBufferHandle;      // 平台特定的内部句柄

        // 仅对 Mapped 类型有效
        uint8_t* host_ptr{nullptr};    // CPU 可访问的指针
        size_t buffer_size{0};
        // 内部使用
        void* platform_data{nullptr};  // 平台私有数据
    };

    struct ImageRect
    {
        int x;        /* upper-left x */
        int y;        /* upper-left y */
        int width;    /* width */
        int height;   /* height */
    };

    // ========== Buffer Management (New Interface) ==========
    
    /**
     * @brief 创建 G2D 缓冲区（新接口）
     * 
     * @param type 缓冲区类型
     * @param params 缓冲区参数
     * 
     * 行为差异：
     * - Hardware 类型：
     *   - RGA: 使用 params.fd/handle
     *   - VIC: 分配 NVBUF_MEM_SURFACE_ARRAY
     * 
     * - Mapped 类型：
     *   - RGA: 包装 params.host_ptr（零拷贝）
     *   - VIC: 分配 NVBUF_MEM_SURFACE_ARRAY + 拷贝 params.host_ptr 数据
     * 
     * @return std::shared_ptr<G2DBuffer>
     */
    virtual std::shared_ptr<G2DBuffer> createBuffer(
        BufferType type,
        const G2DBufferParams& params) = 0;

    /**
     * @brief 释放 G2D 缓冲区（新接口）
     * @param buffer 要释放的缓冲区
     */
    virtual void releaseBuffer(std::shared_ptr<G2DBuffer> buffer) = 0;

    /**
     * @brief 同步 Mapped 缓冲区（关键新增接口！）
     * 
     * 使用场景：
     * 1. CPU 修改了 buffer->host_ptr 数据后，调用 syncBuffer(buf, CpuToDevice)
     * 2. 硬件处理完后，需要 CPU 读取，调用 syncBuffer(buf, DeviceToCpu)
     * 
     * 平台行为：
     * - RGA: 通常是空操作（驱动可能自动处理）
     * - VIC: 
     *   - CpuToDevice: memcpy + NvBufSurfaceSyncForDevice
     *   - DeviceToCpu: NvBufSurfaceSyncForCpu + memcpy
     * 
     * @param buffer 要同步的 Mapped 类型缓冲区
     * @param direction 同步方向
     * @return 0 成功，-1 失败
     * 
     * ⚠️ 注意：仅对 Mapped 类型有效，Hardware 类型返回 -1
     */
    virtual int syncBuffer(
        std::shared_ptr<G2DBuffer> buffer,
        SyncDirection direction) = 0;

    /**
     * @brief 映射 Hardware 缓冲区到 CPU 可访问内存
     * 
     * 使用场景：临时需要 CPU 访问 Hardware 缓冲区
     * 
     * 平台行为：
     * - RGA: 可能需要 ioctl 映射 fd
     * - VIC: NvBufSurfaceMap
     * 
     * @param buffer Hardware 类型缓冲区
     * @param access_mode "read", "write", "readwrite"
     * @return CPU 可访问的指针，失败返回 nullptr
     * 
     * ⚠️ 使用完毕后必须调用 unmapBuffer
     */
    virtual void* mapBuffer(
        std::shared_ptr<G2DBuffer> buffer,
        const std::string& access_mode = "readwrite") = 0;

    /**
     * @brief 解除 Hardware 缓冲区的 CPU 映射
     * @param buffer Hardware 类型缓冲区
     */
    virtual void unmapBuffer(std::shared_ptr<G2DBuffer> buffer) = 0;

    // ========== Platform Capabilities ==========
    
    /**
     * @brief 查询平台能力
     * @param capability 能力名称
     *   - "hardware_draw": 是否支持硬件绘制原语
     *   - "zero_copy_cpu_access": CPU 访问是否零拷贝（RGA: true, VIC: false）
     *   - "requires_explicit_sync": 是否需要显式同步（RGA: false, VIC: true）
     * @return true 支持，false 不支持
     */
    virtual bool queryCapability(const std::string& capability) const = 0;

    /**
     * @brief 获取平台名称
     * @return "rkrga", "nvvic"
     */
    virtual std::string getPlatformName() const = 0;
    // ========== Image Operations ==========

    /**
     * @brief 图像缩放
     * @param src 源缓冲区（Hardware 或 Mapped）
     * @param dst 目标缓冲区（Hardware 或 Mapped）
     * @return 0 成功，-1 失败
     * 
     * ⚠️ 如果 src/dst 是 Mapped 类型且 CPU 修改过，需先调用 syncBuffer(CpuToDevice)
     */
    virtual int imageResize(
        std::shared_ptr<G2DBuffer> src,
        std::shared_ptr<G2DBuffer> dst) = 0;

    /**
     * @brief 图像拷贝
     * @param src 源缓冲区
     * @param dst 目标缓冲区（尺寸和格式必须相同）
     * @return 0 成功，-1 失败
     * 
     * ⚠️ 拷贝后，如需 CPU 读取 dst，需调用 syncBuffer(DeviceToCpu)
     */
    virtual int imageCopy(
        std::shared_ptr<G2DBuffer> src,
        std::shared_ptr<G2DBuffer> dst) = 0;

    /**
     * @brief 颜色空间转换
     * @param src 源缓冲区
     * @param dst 目标缓冲区
     * @param src_format 源格式（如 "YUV420SP"）
     * @param dst_format 目标格式（如 "RGBA8888"）
     * @return 0 成功，-1 失败
     */
    virtual int imageCvtColor(
        std::shared_ptr<G2DBuffer> src,
        std::shared_ptr<G2DBuffer> dst,
        const std::string& src_format,
        const std::string& dst_format) = 0;

    /**
     * @brief 绘制矩形（硬件加速，如果支持）
     * 
     * 平台支持：
     * - RGA: ✅ 支持（imrectangle）
     * - VIC: ❌ 不支持（返回 -1）
     * 
     * @param dst 目标缓冲区
     * @param rect 矩形区域
     * @param color 颜色值（ARGB8888 格式）
     * @param thickness 线条粗细
     * @return 0 成功，-1 失败/不支持
     */
    virtual int imageDrawRectangle(
        std::shared_ptr<G2DBuffer> dst,
        ImageRect& rect,
        uint32_t color,
        int thickness) = 0;

    virtual ~IGraphics2D() = default;

protected:
    IGraphics2D() = default;
    IGraphics2D(const IGraphics2D&) = delete;
    IGraphics2D& operator=(const IGraphics2D&) = delete;
};

} // namespace bsp_g2d

#endif // __IGRAPHICS2D_HPP__