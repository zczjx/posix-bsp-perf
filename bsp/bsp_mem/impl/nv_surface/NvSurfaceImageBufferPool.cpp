#include "NvSurfaceImageBufferPool.hpp"

#include <iostream>
#include <map>

#include "nvbufsurface.h"

namespace bsp_mem
{
namespace impl
{

namespace
{

NvBufSurfaceColorFormat toNvFormat(const std::string& format)
{
    static const std::map<std::string, NvBufSurfaceColorFormat> formatMap = {
        {"RGBA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"BGRA8888", NVBUF_COLOR_FORMAT_BGRA},
        {"ARGB8888", NVBUF_COLOR_FORMAT_ARGB},
        {"XRGB8888", NVBUF_COLOR_FORMAT_xRGB},
        {"YUV420SP", NVBUF_COLOR_FORMAT_NV12},
        {"YCbCr_420_SP", NVBUF_COLOR_FORMAT_NV12},
        {"YCrCb_420_SP", NVBUF_COLOR_FORMAT_NV21},
        {"YUV420P", NVBUF_COLOR_FORMAT_YUV420},
        {"YCbCr_420_P", NVBUF_COLOR_FORMAT_YUV420},
        {"YCrCb_420_P", NVBUF_COLOR_FORMAT_YVU420},
    };
    auto it = formatMap.find(format);
    return it != formatMap.end() ? it->second : NVBUF_COLOR_FORMAT_INVALID;
}

/// NvBufSurface 所有权：析构时解映射并销毁
struct NvSurfaceResource
{
    NvBufSurface* surf{nullptr};
    uint32_t mappedPlanes{0};

    NvSurfaceResource() = default;
    NvSurfaceResource(const NvSurfaceResource&) = delete;
    NvSurfaceResource& operator=(const NvSurfaceResource&) = delete;

    ~NvSurfaceResource()
    {
        if (!surf) {
            return;
        }
        for (uint32_t plane = 0; plane < mappedPlanes; ++plane) {
            NvBufSurfaceUnMap(surf, 0, plane);
        }
        NvBufSurfaceDestroy(surf);
    }
};

} // namespace

std::shared_ptr<ImageBufferPoolBase::Slot> NvSurfaceImageBufferPool::allocateSlot(
    const AcquireRequest& request)
{
    const auto& desc = request.desc;
    const NvBufSurfaceColorFormat colorFormat = toNvFormat(desc.format);
    if (colorFormat == NVBUF_COLOR_FORMAT_INVALID) {
        std::cerr << "NvSurfaceImageBufferPool: unsupported format " << desc.format
                  << " (24-bit RGB/BGR is not consumable by VIC)" << std::endl;
        return nullptr;
    }

    NvBufSurfaceAllocateParams allocParams{};
    allocParams.params.width = desc.width;
    allocParams.params.height = desc.height;
    allocParams.params.layout = NVBUF_LAYOUT_PITCH;
    allocParams.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    allocParams.params.colorFormat = colorFormat;
    allocParams.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

    auto resource = std::make_shared<NvSurfaceResource>();
    if (NvBufSurfaceAllocate(&resource->surf, 1, &allocParams) != 0 || !resource->surf) {
        resource->surf = nullptr;
        std::cerr << "NvSurfaceImageBufferPool: NvBufSurfaceAllocate failed" << std::endl;
        return nullptr;
    }
    resource->surf->numFilled = 1;

    NvBufSurface* surf = resource->surf;
    const NvBufSurfaceParams& sp = surf->surfaceList[0];

    for (uint32_t plane = 0; plane < sp.planeParams.num_planes && plane < 4; ++plane) {
        if (NvBufSurfaceMap(surf, 0, plane, NVBUF_MAP_READ_WRITE) != 0) {
            std::cerr << "NvSurfaceImageBufferPool: map plane " << plane << " failed"
                      << std::endl;
            return nullptr;  // 已映射的 plane 由 resource 析构解映射
        }
        resource->mappedPlanes = plane + 1;
    }

    auto slot = std::make_shared<Slot>();
    auto& view = slot->view;
    view.desc.width = desc.width;
    view.desc.height = desc.height;
    view.desc.widthStride = sp.planeParams.width[0];
    view.desc.heightStride = sp.planeParams.height[0];
    view.desc.format = desc.format;
    view.memoryType = bsp_perf::bsp_image::ImageMemoryType::DmaBuf;
    view.access = bsp_perf::bsp_image::ImageAccess::ReadWrite;
    view.planeCount = resource->mappedPlanes;

    size_t totalSize = 0;
    for (uint32_t plane = 0; plane < resource->mappedPlanes; ++plane) {
        view.planes[plane].data =
            static_cast<uint8_t*>(surf->surfaceList[0].mappedAddr.addr[plane]);
        view.planes[plane].size = sp.planeParams.psize[plane];
        view.planes[plane].offset = sp.planeParams.offset[plane];
        view.planes[plane].rowStride = sp.planeParams.pitch[plane];
        view.planes[plane].fd = static_cast<int>(sp.bufferDesc);
        totalSize += sp.planeParams.psize[plane];
    }
    view.desc.dataSize = totalSize;

    slot->memory = std::move(resource);
    slot->platformHandle = surf;
    return slot;
}

int NvSurfaceImageBufferPool::syncBuffer(bsp_perf::bsp_image::ImageBuffer& buffer,
                                         SyncDirection direction)
{
    NvBufSurface* const* surfPtr = std::any_cast<NvBufSurface*>(&buffer.nativeHandle);
    if (!surfPtr || !*surfPtr) {
        std::cerr << "NvSurfaceImageBufferPool: buffer was not acquired from this pool"
                  << std::endl;
        return -1;
    }
    NvBufSurface* surf = *surfPtr;

    const uint32_t numPlanes = surf->surfaceList[0].planeParams.num_planes;
    const bool toDevice = direction != SyncDirection::DeviceToCpu;
    const bool toCpu = direction != SyncDirection::CpuToDevice;

    for (uint32_t plane = 0; plane < numPlanes; ++plane) {
        if (toDevice && NvBufSurfaceSyncForDevice(surf, 0, plane) != 0) {
            return -1;
        }
        if (toCpu && NvBufSurfaceSyncForCpu(surf, 0, plane) != 0) {
            return -1;
        }
    }
    return 0;
}

} // namespace impl
} // namespace bsp_mem
