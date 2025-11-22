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
    
    std::cout << "NvVicGraphics2D: Destroyed" << std::endl;
}

NvBufSurfaceColorFormat NvVicGraphics2D::mapFormatStringToNvFormat(const std::string& format)
{
    // Map common formats to NvBufSurfaceColorFormat
    // Based on transform_unit_sample and IGraphics2D format strings
    static const std::map<std::string, NvBufSurfaceColorFormat> formatMap = {
        // RGB formats (32-bit only, VIC does not support 24-bit RGB/BGR)
        {"RGBA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"RGBX8888", NVBUF_COLOR_FORMAT_RGBA},
        {"BGRA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"ARGB8888", NVBUF_COLOR_FORMAT_ARGB},
        {"XRGB8888", NVBUF_COLOR_FORMAT_xRGB},
        {"ABGR8888", NVBUF_COLOR_FORMAT_RGBA},
        {"XBGR8888", NVBUF_COLOR_FORMAT_RGBA},
        // Note: RGB888/BGR888 are NOT supported by VIC hardware
        // Use RGBA8888 instead, or use GPU-based transformation
        
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
    // Reset array
    memset(bytes_per_pixel, 0, sizeof(int) * 4);
    
    // Based on transform_unit_sample implementation
    switch (pixel_format)
    {
        case NVBUF_COLOR_FORMAT_NV12:
        case NVBUF_COLOR_FORMAT_NV12_ER:
        case NVBUF_COLOR_FORMAT_NV21:
        case NVBUF_COLOR_FORMAT_NV12_709:
        case NVBUF_COLOR_FORMAT_NV12_709_ER:
        case NVBUF_COLOR_FORMAT_NV12_2020:
        {
            bytes_per_pixel[0] = 1;
            bytes_per_pixel[1] = 2;
            break;
        }
        case NVBUF_COLOR_FORMAT_ARGB:
        case NVBUF_COLOR_FORMAT_xRGB:
        case NVBUF_COLOR_FORMAT_RGBA:
        {
            bytes_per_pixel[0] = 4;
            break;
        }
        case NVBUF_COLOR_FORMAT_YUV420:
        case NVBUF_COLOR_FORMAT_YVU420:
        case NVBUF_COLOR_FORMAT_YUV420_709:
        {
            bytes_per_pixel[0] = 1;
            bytes_per_pixel[1] = 1;
            bytes_per_pixel[2] = 1;
            break;
        }
        case NVBUF_COLOR_FORMAT_UYVY:
        case NVBUF_COLOR_FORMAT_UYVY_ER:
        case NVBUF_COLOR_FORMAT_UYVY_709:
        case NVBUF_COLOR_FORMAT_UYVY_709_ER:
        case NVBUF_COLOR_FORMAT_UYVY_2020:
        {
            bytes_per_pixel[0] = 2;
            break;
        }
        default:
            break;
    }
}

std::shared_ptr<IGraphics2D::G2DBuffer> NvVicGraphics2D::createG2DBuffer(
    const std::string& g2dBufferMapType, 
    G2DBufferParams& params)
{
    auto g2dBuffer = std::make_shared<G2DBuffer>();
    g2dBuffer->g2dPlatform = "nvvic";
    g2dBuffer->g2dBufferMapType = g2dBufferMapType;
    
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
    
    // Handle width_stride if provided
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
    }
    
    // Set G2DBuffer properties
    g2dBuffer->g2dBufferHandle = nvbuf_surf;
    g2dBuffer->rawBufferSize = params.rawBufferSize;
    // 保存 virtual_addr 指针，以便后续写回数据
    g2dBuffer->rawBuffer = static_cast<uint8_t*>(params.virtual_addr);
    
    // If user provides virtual_addr, copy data to NvBufSurface
    if (params.virtual_addr != nullptr && g2dBufferMapType == "virtualaddr") {
        int bytes_per_pixel[4] = {0};
        fillBytesPerPixel(colorFormat, bytes_per_pixel);
        
        unsigned int num_planes = nvbuf_surf->surfaceList[0].planeParams.num_planes;
        // 调试日志已关闭
        // std::cout << "NvVicGraphics2D: Copying virtualaddr data to NvBufSurface" << std::endl;
        
        size_t offset = 0;
        for (unsigned int plane = 0; plane < num_planes; ++plane) {
            ret = NvBufSurfaceMap(nvbuf_surf, 0, plane, NVBUF_MAP_READ_WRITE);
            if (ret == 0) {
                void* dst_addr = (void*)nvbuf_surf->surfaceList[0].mappedAddr.addr[plane];
                uint8_t* src_addr = (uint8_t*)params.virtual_addr + offset;
                
                unsigned int plane_height = nvbuf_surf->surfaceList[0].planeParams.height[plane];
                unsigned int plane_width = nvbuf_surf->surfaceList[0].planeParams.width[plane];
                unsigned int pitch = nvbuf_surf->surfaceList[0].planeParams.pitch[plane];
                
                // Copy line by line to handle pitch correctly
                size_t lines_copied = 0;
                for (unsigned int i = 0; i < plane_height; ++i) {
                    size_t bytes_to_copy = plane_width * bytes_per_pixel[plane];
                    if (offset + bytes_to_copy <= params.rawBufferSize) {
                        memcpy((uint8_t*)dst_addr + i * pitch, src_addr + i * plane_width * bytes_per_pixel[plane], bytes_to_copy);
                        lines_copied++;
                    } else {
                        std::cerr << "NvVicGraphics2D: WARNING: Skipping line " << i << " of plane " << plane 
                                  << " (would exceed rawBufferSize: " << (offset + bytes_to_copy) << " > " << params.rawBufferSize << ")" << std::endl;
                    }
                }
                
                size_t plane_bytes = plane_height * plane_width * bytes_per_pixel[plane];
                offset += plane_bytes;
                
                // Sync for device access (required for VIC)
                NvBufSurfaceSyncForDevice(nvbuf_surf, 0, plane);
                NvBufSurfaceUnMap(nvbuf_surf, 0, plane);
            } else {
                std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << ", ret=" << ret << std::endl;
            }
        }
    }
    
    std::cout << "NvVicGraphics2D: Created buffer " << params.width << "x" << params.height 
              << " format: " << params.format << std::endl;
    
    return g2dBuffer;
}

void NvVicGraphics2D::releaseG2DBuffer(std::shared_ptr<G2DBuffer> g2dBuffer)
{
    if (!g2dBuffer) return;
    
    std::lock_guard<std::mutex> lock(m_mapMutex);
    
    auto it = m_bufferMap.find(g2dBuffer.get());
    if (it != m_bufferMap.end()) {
        if (it->second) {
            NvBufSurfaceDestroy(it->second);
        }
        m_bufferMap.erase(it);
        std::cout << "NvVicGraphics2D: Released buffer" << std::endl;
    }
}

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

int NvVicGraphics2D::performTransform(
    NvBufSurface* src, 
    NvBufSurface* dst,
    NvBufSurfTransformParams& transform_params)
{
    if (!src || !dst) {
        std::cerr << "NvVicGraphics2D: Invalid source or destination buffer" << std::endl;
        return -1;
    }
    
    // Perform the transformation using VIC
    int ret = NvBufSurfTransform(src, dst, &transform_params);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Transform failed with error: " << ret << std::endl;
        return -1;
    }
    
    return 0;
}

int NvVicGraphics2D::imageResize(std::shared_ptr<G2DBuffer> src, std::shared_ptr<G2DBuffer> dst)
{
    NvBufSurface* src_surf = getNvBufSurface(src);
    NvBufSurface* dst_surf = getNvBufSurface(dst);
    
    if (!src_surf || !dst_surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for resize" << std::endl;
        return -1;
    }
    
    // Setup transform parameters for resize
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

    // 调试日志已关闭（检测成功后）
    // 如需调试，取消注释下面的代码

    // Use NvBufSurfaceCopy for direct copy
    int ret = NvBufSurfaceCopy(src_surf, dst_surf);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Copy failed with error: " << ret << std::endl;
        return -1;
    }

    // ⚠️ 关键修复：如果目标是 virtualaddr 类型，需要将数据从 NvBufSurface 复制回用户缓冲区
    // 这在 createG2DBuffer 的反向操作中是缺失的
    if (dst->g2dBufferMapType == "virtualaddr" && dst->rawBuffer != nullptr) {
        // Map, sync, and copy data back to host buffer
        NvBufSurfaceColorFormat colorFormat = dst_surf->surfaceList[0].colorFormat;
        int bytes_per_pixel[4] = {0};
        fillBytesPerPixel(colorFormat, bytes_per_pixel);

        unsigned int num_planes = dst_surf->surfaceList[0].planeParams.num_planes;

        size_t offset = 0;
        for (unsigned int plane = 0; plane < num_planes; ++plane) {
            ret = NvBufSurfaceMap(dst_surf, 0, plane, NVBUF_MAP_READ);
            if (ret == 0) {
                // Sync from device to CPU
                NvBufSurfaceSyncForCpu(dst_surf, 0, plane);

                void* src_addr = (void*)dst_surf->surfaceList[0].mappedAddr.addr[plane];
                uint8_t* dst_addr = dst->rawBuffer + offset;

                unsigned int plane_height = dst_surf->surfaceList[0].planeParams.height[plane];
                unsigned int plane_width = dst_surf->surfaceList[0].planeParams.width[plane];
                unsigned int pitch = dst_surf->surfaceList[0].planeParams.pitch[plane];

                // Copy line by line from NvBufSurface to user buffer
                for (unsigned int i = 0; i < plane_height; ++i) {
                    size_t bytes_to_copy = plane_width * bytes_per_pixel[plane];
                    memcpy(dst_addr + i * plane_width * bytes_per_pixel[plane],
                           (uint8_t*)src_addr + i * pitch,
                           bytes_to_copy);
                }

                offset += plane_height * plane_width * bytes_per_pixel[plane];

                NvBufSurfaceUnMap(dst_surf, 0, plane);
            } else {
                std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << ", ret=" << ret << std::endl;
            }
        }
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
    std::cerr << "NvVicGraphics2D: VIC is designed for format conversion, scaling, and compositing, not primitive drawing" << std::endl;
    // VIC doesn't support drawing primitives
    // This would require CPU-based implementation or using another API (like Cairo or OpenCV)
    return -1;
}

int NvVicGraphics2D::imageCvtColor(
    std::shared_ptr<G2DBuffer> src, 
    std::shared_ptr<G2DBuffer> dst,
    const std::string& src_format, 
    const std::string& dst_format)
{
    NvBufSurface* src_surf = getNvBufSurface(src);
    NvBufSurface* dst_surf = getNvBufSurface(dst);
    
    if (!src_surf || !dst_surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for color conversion" << std::endl;
        return -1;
    }
    
    // Verify format compatibility
    NvBufSurfaceColorFormat src_nv_format = mapFormatStringToNvFormat(src_format);
    NvBufSurfaceColorFormat dst_nv_format = mapFormatStringToNvFormat(dst_format);
    
    if (src_nv_format == NVBUF_COLOR_FORMAT_INVALID || 
        dst_nv_format == NVBUF_COLOR_FORMAT_INVALID) {
        std::cerr << "NvVicGraphics2D: Invalid format for color conversion" << std::endl;
        return -1;
    }
    
    // Setup transform parameters for color conversion
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

