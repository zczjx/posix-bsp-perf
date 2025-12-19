#include "NvVicGraphics2D.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace bsp_g2d
{

NvVicGraphics2D::NvVicGraphics2D()
    : m_sessionInitialized(false)
{
    // Initialize VIC session with default parameters
    NvBufSurfTransformConfigParams config_params = {
        NvBufSurfTransformCompute_VIC, 
        0, 
        nullptr
    };
    
    int ret = NvBufSurfTransformSetSessionParams(&config_params);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Failed to initialize VIC session: " << ret << std::endl;
        throw std::runtime_error("Failed to initialize VIC session");
    }
    
    m_sessionInitialized = true;
    std::cout << "NvVicGraphics2D: VIC session initialized successfully" << std::endl;
}

NvVicGraphics2D::~NvVicGraphics2D()
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    
    // Cleanup all remaining buffers
    for (auto& pair : m_bufferMap) {
        if (pair.second) {
            NvBufSurfaceDestroy(pair.second);
        }
    }
    m_bufferMap.clear();
    m_mapInfo.clear();
    
    std::cout << "NvVicGraphics2D: Destroyed" << std::endl;
}

NvBufSurfaceColorFormat NvVicGraphics2D::mapFormatStringToNvFormat(const std::string& format)
{
    // Map common formats to NvBufSurfaceColorFormat
    static const std::map<std::string, NvBufSurfaceColorFormat> formatMap = {
        // RGB formats (32-bit only, VIC does not support 24-bit RGB/BGR)
        {"RGBA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"RGBX8888", NVBUF_COLOR_FORMAT_RGBA},
        {"BGRA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"ARGB8888", NVBUF_COLOR_FORMAT_ARGB},
        {"XRGB8888", NVBUF_COLOR_FORMAT_xRGB},
        {"ABGR8888", NVBUF_COLOR_FORMAT_RGBA},
        {"XBGR8888", NVBUF_COLOR_FORMAT_RGBA},
        
        // YUV 4:2:0 formats
        {"YCbCr_420_SP", NVBUF_COLOR_FORMAT_NV12},
        {"YUV420SP", NVBUF_COLOR_FORMAT_NV12},
        {"YCbCr_420_P", NVBUF_COLOR_FORMAT_YUV420},
        {"YUV420P", NVBUF_COLOR_FORMAT_YUV420},
        {"YCrCb_420_SP", NVBUF_COLOR_FORMAT_NV21},
        {"YCrCb_420_P", NVBUF_COLOR_FORMAT_YVU420},
        
        // YUV 4:2:2 formats
        {"YUYV_422", NVBUF_COLOR_FORMAT_UYVY},
        {"UYVY_422", NVBUF_COLOR_FORMAT_UYVY},
        {"YCbCr_422_SP", NVBUF_COLOR_FORMAT_NV12},
        {"YCbCr_422_P", NVBUF_COLOR_FORMAT_YUV420},
    };
    
    auto it = formatMap.find(format);
    if (it != formatMap.end()) {
        return it->second;
    }
    
    std::cerr << "NvVicGraphics2D: Unsupported format: " << format << std::endl;
    return NVBUF_COLOR_FORMAT_INVALID;
}

void NvVicGraphics2D::fillBytesPerPixel(NvBufSurfaceColorFormat pixel_format, int* bytes_per_pixel)
{
    memset(bytes_per_pixel, 0, sizeof(int) * 4);
    
    switch (pixel_format)
    {
        case NVBUF_COLOR_FORMAT_NV12:
        case NVBUF_COLOR_FORMAT_NV12_ER:
        case NVBUF_COLOR_FORMAT_NV21:
        case NVBUF_COLOR_FORMAT_NV12_709:
        case NVBUF_COLOR_FORMAT_NV12_709_ER:
        case NVBUF_COLOR_FORMAT_NV12_2020:
            bytes_per_pixel[0] = 1;
            bytes_per_pixel[1] = 2;
            break;
        case NVBUF_COLOR_FORMAT_ARGB:
        case NVBUF_COLOR_FORMAT_xRGB:
        case NVBUF_COLOR_FORMAT_RGBA:
            bytes_per_pixel[0] = 4;
            break;
        case NVBUF_COLOR_FORMAT_YUV420:
        case NVBUF_COLOR_FORMAT_YVU420:
        case NVBUF_COLOR_FORMAT_YUV420_709:
            bytes_per_pixel[0] = 1;
            bytes_per_pixel[1] = 1;
            bytes_per_pixel[2] = 1;
            break;
        case NVBUF_COLOR_FORMAT_UYVY:
        case NVBUF_COLOR_FORMAT_UYVY_ER:
        case NVBUF_COLOR_FORMAT_UYVY_709:
        case NVBUF_COLOR_FORMAT_UYVY_709_ER:
        case NVBUF_COLOR_FORMAT_UYVY_2020:
            bytes_per_pixel[0] = 2;
            break;
        default:
            break;
    }
}

int NvVicGraphics2D::copyHostToNvBufSurface(void* host_ptr, size_t buffer_size, NvBufSurface* surf)
{
    if (!host_ptr || !surf || buffer_size == 0) {
        return -1;
    }

    NvBufSurfaceColorFormat colorFormat = surf->surfaceList[0].colorFormat;
    int bytes_per_pixel[4] = {0};
    fillBytesPerPixel(colorFormat, bytes_per_pixel);
    
    unsigned int num_planes = surf->surfaceList[0].planeParams.num_planes;
    size_t offset = 0;
    
    for (unsigned int plane = 0; plane < num_planes; ++plane) {
        int ret = NvBufSurfaceMap(surf, 0, plane, NVBUF_MAP_READ_WRITE);
        if (ret == 0) {
            void* dst_addr = (void*)surf->surfaceList[0].mappedAddr.addr[plane];
            uint8_t* src_addr = (uint8_t*)host_ptr + offset;
            
            unsigned int plane_height = surf->surfaceList[0].planeParams.height[plane];
            unsigned int plane_width = surf->surfaceList[0].planeParams.width[plane];
            unsigned int pitch = surf->surfaceList[0].planeParams.pitch[plane];
            
            // Copy line by line to handle pitch correctly
            for (unsigned int i = 0; i < plane_height; ++i) {
                size_t bytes_to_copy = plane_width * bytes_per_pixel[plane];
                if (offset + bytes_to_copy <= buffer_size) {
                    memcpy((uint8_t*)dst_addr + i * pitch, 
                           src_addr + i * plane_width * bytes_per_pixel[plane], 
                           bytes_to_copy);
                }
            }
            
            size_t plane_bytes = plane_height * plane_width * bytes_per_pixel[plane];
            offset += plane_bytes;
            
            // Sync for device access (required for VIC)
            NvBufSurfaceSyncForDevice(surf, 0, plane);
            NvBufSurfaceUnMap(surf, 0, plane);
        } else {
            std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << std::endl;
            return -1;
        }
    }
    
    return 0;
}

int NvVicGraphics2D::copyNvBufSurfaceToHost(NvBufSurface* surf, void* host_ptr, size_t buffer_size)
{
    if (!surf || !host_ptr || buffer_size == 0) {
        return -1;
    }

    NvBufSurfaceColorFormat colorFormat = surf->surfaceList[0].colorFormat;
    int bytes_per_pixel[4] = {0};
    fillBytesPerPixel(colorFormat, bytes_per_pixel);
    
    unsigned int num_planes = surf->surfaceList[0].planeParams.num_planes;
    size_t offset = 0;
    
    for (unsigned int plane = 0; plane < num_planes; ++plane) {
        int ret = NvBufSurfaceMap(surf, 0, plane, NVBUF_MAP_READ);
        if (ret == 0) {
            // Sync from device to CPU
            NvBufSurfaceSyncForCpu(surf, 0, plane);

            void* src_addr = (void*)surf->surfaceList[0].mappedAddr.addr[plane];
            uint8_t* dst_addr = (uint8_t*)host_ptr + offset;

            unsigned int plane_height = surf->surfaceList[0].planeParams.height[plane];
            unsigned int plane_width = surf->surfaceList[0].planeParams.width[plane];
            unsigned int pitch = surf->surfaceList[0].planeParams.pitch[plane];

            // Copy line by line from NvBufSurface to user buffer
            for (unsigned int i = 0; i < plane_height; ++i) {
                size_t bytes_to_copy = plane_width * bytes_per_pixel[plane];
                if (offset + bytes_to_copy <= buffer_size) {
                    memcpy(dst_addr + i * plane_width * bytes_per_pixel[plane],
                           (uint8_t*)src_addr + i * pitch,
                           bytes_to_copy);
                }
            }

            offset += plane_height * plane_width * bytes_per_pixel[plane];

            NvBufSurfaceUnMap(surf, 0, plane);
        } else {
            std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << std::endl;
            return -1;
        }
    }

    return 0;
}

// ========== New Interface Implementation ==========

std::shared_ptr<IGraphics2D::G2DBuffer> NvVicGraphics2D::createBuffer(
    BufferType type,
    const G2DBufferParams& params)
{
    auto g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->g2dPlatform = "nvvic";
    g2dBuffer->bufferType = type;

    // Map format to NvBufSurfaceColorFormat
    NvBufSurfaceColorFormat colorFormat = mapFormatStringToNvFormat(params.format);
    if (colorFormat == NVBUF_COLOR_FORMAT_INVALID) {
        std::cerr << "NvVicGraphics2D: Invalid color format: " << params.format << std::endl;
        return nullptr;
    }

    // Create NvBufSurface with proper parameters
    NvBufSurfaceAllocateParams allocParams = {{0}};
    allocParams.params.width = params.width;
    allocParams.params.height = params.height;
    allocParams.params.layout = NVBUF_LAYOUT_PITCH;
    allocParams.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    allocParams.params.colorFormat = colorFormat;
    allocParams.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

    if (params.width_stride > 0) {
        allocParams.params.width = params.width_stride;
    }

    NvBufSurface* nvbuf_surf = nullptr;
    int ret = NvBufSurfaceAllocate(&nvbuf_surf, 1, &allocParams);
    if (ret != 0 || !nvbuf_surf) {
        std::cerr << "NvVicGraphics2D: Failed to allocate NvBufSurface: " << ret << std::endl;
        return nullptr;
    }

    nvbuf_surf->numFilled = 1;

    // Store the mapping
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_bufferMap[g2dBuffer.get()] = nvbuf_surf;
        m_mapInfo[g2dBuffer.get()] = {nullptr, false};
    }

    g2dBuffer->g2dBufferHandle = nvbuf_surf;

    // Handle Mapped type
    if (type == BufferType::Mapped)
    {
        if (params.host_ptr == nullptr)
        {
            std::cerr << "NvVicGraphics2D: Mapped buffer requires host_ptr" << std::endl;
            return nullptr;
        }

        if (params.host_ptr && params.buffer_size > 0)
        {
            // Copy initial data to NvBufSurface
            ret = copyHostToNvBufSurface(params.host_ptr, params.buffer_size, nvbuf_surf);
            if (ret != 0)
            {
                std::cerr << "NvVicGraphics2D: Failed to copy host data to NvBufSurface" << std::endl;
                return nullptr;
            }
            // Store host pointer for future sync operations
            g2dBuffer->host_ptr = static_cast<uint8_t*>(params.host_ptr);
            g2dBuffer->buffer_size = params.buffer_size;
        }
    }

    std::cout << "NvVicGraphics2D: Created " << (type == BufferType::Hardware ? "Hardware" : "Mapped")
              << " buffer " << params.width << "x" << params.height
              << " format: " << params.format << std::endl;

    return g2dBuffer;
}

void NvVicGraphics2D::releaseBuffer(std::shared_ptr<G2DBuffer> buffer)
{
    if (!buffer) return;

    std::lock_guard<std::mutex> lock(m_mapMutex);

    auto it = m_bufferMap.find(buffer.get());
    if (it != m_bufferMap.end())
    {
        if (it->second)
        {
            NvBufSurfaceDestroy(it->second);
        }
        m_bufferMap.erase(it);
    }

    auto it_map = m_mapInfo.find(buffer.get());

    if (it_map != m_mapInfo.end())
    {
        m_mapInfo.erase(it_map);
    }

    std::cout << "NvVicGraphics2D: Released buffer" << std::endl;
}

int NvVicGraphics2D::syncBuffer(
    std::shared_ptr<G2DBuffer> buffer,
    SyncDirection direction)
{
    if (!buffer || buffer->bufferType != BufferType::Mapped) {
        std::cerr << "NvVicGraphics2D: syncBuffer only works with Mapped buffers" << std::endl;
        return -1;
    }
    
    if (!buffer->host_ptr || buffer->buffer_size == 0) {
        std::cerr << "NvVicGraphics2D: Invalid host pointer or buffer size" << std::endl;
        return -1;
    }
    
    NvBufSurface* surf = getNvBufSurface(buffer);
    if (!surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface" << std::endl;
        return -1;
    }
    
    int ret = 0;
    
    if (direction == SyncDirection::CpuToDevice || 
        direction == SyncDirection::Bidirectional) {
        // CPU → Device: Copy host_ptr to NvBufSurface
        ret = copyHostToNvBufSurface(buffer->host_ptr, buffer->buffer_size, surf);
        if (ret != 0) {
            std::cerr << "NvVicGraphics2D: Failed to sync CPU to Device" << std::endl;
            return -1;
        }
        std::cout << "NvVicGraphics2D: Synced CPU → Device" << std::endl;
    }
    
    if (direction == SyncDirection::DeviceToCpu || 
        direction == SyncDirection::Bidirectional) {
        // Device → CPU: Copy NvBufSurface to host_ptr
        ret = copyNvBufSurfaceToHost(surf, buffer->host_ptr, buffer->buffer_size);
        if (ret != 0) {
            std::cerr << "NvVicGraphics2D: Failed to sync Device to CPU" << std::endl;
            return -1;
        }
        std::cout << "NvVicGraphics2D: Synced Device → CPU" << std::endl;
    }
    
    return 0;
}

void* NvVicGraphics2D::mapBuffer(
    std::shared_ptr<G2DBuffer> buffer,
    const std::string& access_mode)
{
    if (!buffer || buffer->bufferType != BufferType::Hardware) {
        std::cerr << "NvVicGraphics2D: mapBuffer only works with Hardware buffers" << std::endl;
        return nullptr;
    }
    
    NvBufSurface* surf = getNvBufSurface(buffer);
    if (!surf) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mapMutex);
    
    auto it = m_mapInfo.find(buffer.get());
    if (it != m_mapInfo.end() && it->second.is_mapped) {
        std::cerr << "NvVicGraphics2D: Buffer already mapped" << std::endl;
        return it->second.mapped_addr;
    }
    
    // Determine map flags
    NvBufSurfaceMemMapFlags map_flags = NVBUF_MAP_READ_WRITE;
    if (access_mode == "read") {
        map_flags = NVBUF_MAP_READ;
    } else if (access_mode == "write") {
        map_flags = NVBUF_MAP_WRITE;
    }
    
    // Map plane 0 (for simplicity, only map first plane)
    int ret = NvBufSurfaceMap(surf, 0, 0, map_flags);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Failed to map buffer" << std::endl;
        return nullptr;
    }
    
    void* mapped_addr = (void*)surf->surfaceList[0].mappedAddr.addr[0];
    m_mapInfo[buffer.get()] = {mapped_addr, true};
    
    return mapped_addr;
}

void NvVicGraphics2D::unmapBuffer(std::shared_ptr<G2DBuffer> buffer)
{
    if (!buffer) return;
    
    NvBufSurface* surf = getNvBufSurface(buffer);
    if (!surf) return;
    
    std::lock_guard<std::mutex> lock(m_mapMutex);
    
    auto it = m_mapInfo.find(buffer.get());
    if (it != m_mapInfo.end() && it->second.is_mapped) {
        NvBufSurfaceUnMap(surf, 0, 0);
        it->second.is_mapped = false;
        it->second.mapped_addr = nullptr;
    }
}

bool NvVicGraphics2D::queryCapability(const std::string& capability) const
{
    if (capability == "hardware_draw") return false;
    if (capability == "zero_copy_cpu_access") return false;
    if (capability == "requires_explicit_sync") return true;
    return false;
}

std::string NvVicGraphics2D::getPlatformName() const
{
    return "nvvic";
}

// ========== Helper Methods ==========

NvBufSurface* NvVicGraphics2D::getNvBufSurface(std::shared_ptr<G2DBuffer> g2dBuffer)
{
    if (!g2dBuffer) return nullptr;

    std::lock_guard<std::mutex> lock(m_mapMutex);

    auto it = m_bufferMap.find(g2dBuffer.get());
    if (it != m_bufferMap.end()) {
        return it->second;
    }

    return nullptr;
}

int NvVicGraphics2D::performTransform(NvBufSurface* src, NvBufSurface* dst, NvBufSurfTransformParams& transform_params)
{
    if (!src || !dst)
    {
        std::cerr << "NvVicGraphics2D: Invalid source or destination buffer" << std::endl;
        return -1;
    }

    int ret = NvBufSurfTransform(src, dst, &transform_params);

    if (ret != 0)
    {
        std::cerr << "NvVicGraphics2D: Transform failed with error: " << ret << std::endl;
        return -1;
    }

    return 0;
}

// ========== Image Operations ==========

int NvVicGraphics2D::imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    NvBufSurface* src_surf = getNvBufSurface(src);
    NvBufSurface* dst_surf = getNvBufSurface(dst);
    
    if (!src_surf || !dst_surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for resize" << std::endl;
        return -1;
    }
    
    NvBufSurfTransformRect src_rect = {0};
    NvBufSurfTransformRect dst_rect = {0};
    
    src_rect.top = 0;
    src_rect.left = 0;
    src_rect.width = src_surf->surfaceList[0].width;
    src_rect.height = src_surf->surfaceList[0].height;
    
    dst_rect.top = 0;
    dst_rect.left = 0;
    dst_rect.width = dst_surf->surfaceList[0].width;
    dst_rect.height = dst_surf->surfaceList[0].height;
    
    NvBufSurfTransformParams transform_params = {0};
    transform_params.transform_flag = NVBUFSURF_TRANSFORM_FILTER;
    transform_params.transform_filter = NvBufSurfTransformInter_Algo4;
    transform_params.src_rect = &src_rect;
    transform_params.dst_rect = &dst_rect;

    std::cout << "NvVicGraphics2D: Resizing from " << src_rect.width << "x" << src_rect.height
              << " to " << dst_rect.width << "x" << dst_rect.height << std::endl;

    return performTransform(src_surf, dst_surf, transform_params);
}

int NvVicGraphics2D::imageCopy(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    NvBufSurface* src_surf = getNvBufSurface(src);
    NvBufSurface* dst_surf = getNvBufSurface(dst);

    if (!src_surf || !dst_surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for copy" << std::endl;
        return -1;
    }

    int ret = NvBufSurfaceCopy(src_surf, dst_surf);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Copy failed with error: " << ret << std::endl;
        return -1;
    }

    std::cout << "NvVicGraphics2D: Image copied successfully" << std::endl;
    return 0;
}

int NvVicGraphics2D::imageDrawRectangle(
    std::shared_ptr<G2DBuffer> dst,
    ImageRect& rect,
    uint32_t color,
    int thickness)
{
    std::cerr << "NvVicGraphics2D: imageDrawRectangle not supported by VIC hardware" << std::endl;
    std::cerr << "NvVicGraphics2D: VIC is designed for format conversion, scaling, and compositing" << std::endl;
    return -1;
}

int NvVicGraphics2D::imageCvtColor(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst,
        const std::string& src_format, const std::string& dst_format)
{
    NvBufSurface* src_surf = getNvBufSurface(src);
    NvBufSurface* dst_surf = getNvBufSurface(dst);

    if (!src_surf || !dst_surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for color conversion" << std::endl;
        return -1;
    }

    NvBufSurfaceColorFormat src_nv_format = mapFormatStringToNvFormat(src_format);
    NvBufSurfaceColorFormat dst_nv_format = mapFormatStringToNvFormat(dst_format);

    if (src_nv_format == NVBUF_COLOR_FORMAT_INVALID ||
        dst_nv_format == NVBUF_COLOR_FORMAT_INVALID)
    {
        std::cerr << "NvVicGraphics2D: Invalid format for color conversion" << std::endl;
        return -1;
    }

    NvBufSurfTransformRect src_rect = {0};
    NvBufSurfTransformRect dst_rect = {0};

    src_rect.top = 0;
    src_rect.left = 0;
    src_rect.width = src_surf->surfaceList[0].width;
    src_rect.height = src_surf->surfaceList[0].height;

    dst_rect.top = 0;
    dst_rect.left = 0;
    dst_rect.width = dst_surf->surfaceList[0].width;
    dst_rect.height = dst_surf->surfaceList[0].height;

    NvBufSurfTransformParams transform_params = {0};
    transform_params.transform_flag = NVBUFSURF_TRANSFORM_FILTER;
    transform_params.transform_filter = NvBufSurfTransformInter_Algo4;
    transform_params.src_rect = &src_rect;
    transform_params.dst_rect = &dst_rect;

    std::cout << "NvVicGraphics2D: Converting color from " << src_format
              << " to " << dst_format << std::endl;

    return performTransform(src_surf, dst_surf, transform_params);
}

} // namespace bsp_g2d
