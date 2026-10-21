/**
 * image_buffer_pool 示例：演示 bsp_mem::IImageBufferPool 的语义
 *   1. acquire() 拿到带 fd 的硬件可访问缓冲（rk_dmaheap / nv_surface）或 host 缓冲
 *   2. 池满且无空闲槽位时返回 nullptr
 *   3. ImageBuffer 最后一个引用析构时槽位自动还池，同规格 acquire 复用同一块内存
 *   4. 池满后换分辨率：空闲槽位会被回收重新分配，不会卡死
 *
 * 用法：image_buffer_pool [--backend auto|host|rk_dmaheap|nv_surface]
 *                         [--width 1280] [--height 720] [--format RGBA8888]
 */

#include <bsp_mem/IImageBufferPool.hpp>

#include <cstring>
#include <iostream>
#include <string>

using bsp_mem::AcquireRequest;
using bsp_mem::IImageBufferPool;
using bsp_mem::PoolConfig;
using bsp_mem::SyncDirection;
using bsp_perf::bsp_image::ImageBuffer;
using bsp_perf::bsp_image::ImageUsage;

namespace
{

struct Options
{
    std::string backend{"auto"};
    uint32_t width{1280};
    uint32_t height{720};
    std::string format{"RGBA8888"};
};

Options parseArgs(int argc, char** argv)
{
    Options opts;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--backend") {
            opts.backend = value;
        } else if (key == "--width") {
            opts.width = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "--height") {
            opts.height = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "--format") {
            opts.format = value;
        }
    }
    return opts;
}

void printBuffer(const char* tag, const ImageBuffer& buffer)
{
    std::cout << tag << " " << buffer.view.desc.width << "x" << buffer.view.desc.height
              << " " << buffer.view.desc.format << " planes=" << buffer.view.planeCount
              << " data=" << static_cast<const void*>(buffer.view.data())
              << " fd=" << buffer.view.planes[0].fd
              << " rowStride=" << buffer.view.planes[0].rowStride
              << " size=" << buffer.view.desc.dataSize << std::endl;
}

} // namespace

int main(int argc, char** argv)
{
    const Options opts = parseArgs(argc, argv);

    PoolConfig config{};
    config.capacity = 2;  // 故意调小，演示池满语义

    auto pool = IImageBufferPool::create(opts.backend, config);
    if (!pool) {
        std::cerr << "failed to create pool backend=" << opts.backend << std::endl;
        return 1;
    }
    std::cout << "pool backend: " << pool->getBackendName()
              << " capacity: " << pool->capacity() << std::endl;

    AcquireRequest request{};
    request.desc.width = opts.width;
    request.desc.height = opts.height;
    request.desc.format = opts.format;
    request.usage = ImageUsage::G2d | ImageUsage::CpuRead | ImageUsage::CpuWrite;

    auto first = pool->acquire(request);
    auto second = pool->acquire(request);
    if (!first || !second) {
        std::cerr << "acquire failed" << std::endl;
        return 1;
    }
    printBuffer("[first ]", *first);
    printBuffer("[second]", *second);

    if (pool->acquire(request)) {
        std::cerr << "pool should be full" << std::endl;
        return 1;
    }
    std::cout << "[pool-full] third acquire returned nullptr (expected)" << std::endl;

    std::memset(first->view.data(), 0x5A, first->view.planes[0].size);
    if (pool->syncBuffer(*first, SyncDirection::CpuToDevice) != 0 ||
        pool->syncBuffer(*first, SyncDirection::DeviceToCpu) != 0) {
        std::cerr << "syncBuffer failed" << std::endl;
        return 1;
    }
    if (first->view.data()[0] != 0x5A) {
        std::cerr << "cpu pattern mismatch" << std::endl;
        return 1;
    }
    std::cout << "[cpu-rw  ] pattern OK" << std::endl;

    const void* firstData = first->view.data();
    first.reset();
    auto reused = pool->acquire(request);
    if (!reused || reused->view.data() != firstData) {
        std::cerr << "slot was not reused" << std::endl;
        return 1;
    }
    std::cout << "[reuse   ] same memory as first: yes (expected)" << std::endl;

    // 池里 second 仍占用 1 槽，reused 占 1 槽。释放 reused 后换分辨率，
    // 空闲槽位应被回收重新分配
    reused.reset();
    AcquireRequest other = request;
    other.desc.width = opts.width / 2;
    other.desc.height = opts.height / 2;
    auto resized = pool->acquire(other);
    if (!resized) {
        std::cerr << "acquire after geometry change failed" << std::endl;
        return 1;
    }
    printBuffer("[resized]", *resized);
    std::cout << "[recycle ] free slot recycled for new geometry: yes (expected)" << std::endl;
    return 0;
}
