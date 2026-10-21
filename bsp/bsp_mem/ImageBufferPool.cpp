#include <bsp_mem/IImageBufferPool.hpp>

#include "impl/host/HostImageBufferPool.hpp"

#ifdef BUILD_PLATFORM_RK35XX
#include "impl/rk_dmaheap/RkDmaHeapImageBufferPool.hpp"
#endif

#ifdef BUILD_PLATFORM_JETSON
#include "impl/nv_surface/NvSurfaceImageBufferPool.hpp"
#endif

#include <iostream>

namespace bsp_mem
{

namespace
{

const char* defaultBackend()
{
#if defined(BUILD_PLATFORM_RK35XX)
    return "rk_dmaheap";
#elif defined(BUILD_PLATFORM_JETSON)
    return "nv_surface";
#else
    return "host";
#endif
}

} // namespace

std::unique_ptr<IImageBufferPool> IImageBufferPool::create(
    const std::string& backend, const PoolConfig& config)
{
    const std::string name =
        (backend.empty() || backend == "auto") ? defaultBackend() : backend;

    if (name == "host") {
        return std::make_unique<impl::HostImageBufferPool>(config);
    }
#if defined(BUILD_PLATFORM_RK35XX)
    if (name == "rk_dmaheap") {
        return std::make_unique<impl::RkDmaHeapImageBufferPool>(config);
    }
#endif
#if defined(BUILD_PLATFORM_JETSON)
    if (name == "nv_surface") {
        return std::make_unique<impl::NvSurfaceImageBufferPool>(config);
    }
#endif

    std::cerr << "IImageBufferPool: backend '" << name
              << "' is not available on this platform" << std::endl;
    return nullptr;
}

} // namespace bsp_mem
