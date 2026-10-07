#include "NvVicGraphics2D.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace bsp_g2d
{

namespace
{

// ========== Buffer ownership ==========

/**
 * Owns the NvBufSurface behind an ImageBuffer. Stored in
 * G2DBufferInternal::platformData, so the surface lives exactly as long as the
 * ImageBuffer (or until releaseBuffer()).
 */
struct NvVicSurfaceResource
{
    NvBufSurface* surf{nullptr};
    void* mappedAddr{nullptr}; // plane-0 mapping created by mapBuffer()
    bool mapped{false};

    NvVicSurfaceResource() = default;
    NvVicSurfaceResource(const NvVicSurfaceResource&) = delete;
    NvVicSurfaceResource& operator=(const NvVicSurfaceResource&) = delete;

    ~NvVicSurfaceResource()
    {
        if (!surf) {
            return;
        }
        if (mapped) {
            NvBufSurfaceUnMap(surf, 0, 0);
        }
        NvBufSurfaceDestroy(surf);
    }
};

std::shared_ptr<NvVicSurfaceResource> resourceOf(
    const std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>& buffer)
{
    auto internal = impl::getG2DBufferInternal(buffer);
    if (!internal || internal->g2dPlatform != "nvvic") {
        return nullptr;
    }
    return std::static_pointer_cast<NvVicSurfaceResource>(internal->platformData);
}

NvBufSurface* surfaceOf(const std::shared_ptr<bsp_perf::bsp_image::ImageBuffer>& buffer)
{
    auto resource = resourceOf(buffer);
    return resource ? resource->surf : nullptr;
}

// ========== Formats ==========

NvBufSurfaceColorFormat mapFormatStringToNvFormat(const std::string& format)
{
    // VIC does not support 24-bit RGB/BGR, only 32-bit RGB variants and YUV.
    static const std::map<std::string, NvBufSurfaceColorFormat> formatMap = {
        {"RGBA8888", NVBUF_COLOR_FORMAT_RGBA},
        {"RGBX8888", NVBUF_COLOR_FORMAT_RGBA},
        {"BGRA8888", NVBUF_COLOR_FORMAT_BGRA},
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

void fillBytesPerPixel(NvBufSurfaceColorFormat pixel_format, int* bytes_per_pixel)
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
        case NVBUF_COLOR_FORMAT_BGRA:
        case NVBUF_COLOR_FORMAT_ABGR:
        case NVBUF_COLOR_FORMAT_RGBx:
        case NVBUF_COLOR_FORMAT_BGRx:
        case NVBUF_COLOR_FORMAT_xBGR:
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

/** True for the 4-byte packed formats the CPU fallbacks understand. */
bool isRgba32(NvBufSurfaceColorFormat format)
{
    return format == NVBUF_COLOR_FORMAT_RGBA || format == NVBUF_COLOR_FORMAT_BGRA;
}

// ========== Host <-> NvBufSurface copies ==========

/** Size of the tightly packed host representation of a surface. */
size_t tightHostSize(NvBufSurface* surf)
{
    const NvBufSurfaceParams& sp = surf->surfaceList[0];
    int bpp[4] = {0};
    fillBytesPerPixel(sp.colorFormat, bpp);

    size_t total = 0;
    for (unsigned int plane = 0; plane < sp.planeParams.num_planes && plane < 4; ++plane) {
        total += static_cast<size_t>(sp.planeParams.width[plane]) * bpp[plane] * sp.planeParams.height[plane];
    }
    return total;
}

/**
 * Host row stride of plane 0 in bytes. A caller supplied row stride is only
 * honoured when it can hold a full row (it is a byte stride); otherwise the
 * rows are assumed to be tightly packed.
 */
size_t hostRowStride(unsigned int plane, size_t tightBytes, uint32_t callerStride)
{
    if (plane == 0 && callerStride >= tightBytes) {
        return callerStride;
    }
    return tightBytes;
}

int copyHostToSurface(const uint8_t* hostPtr, size_t hostSize, uint32_t callerStride, NvBufSurface* surf)
{
    if (!hostPtr || !surf || hostSize == 0) {
        return -1;
    }

    const NvBufSurfaceParams& sp = surf->surfaceList[0];
    int bpp[4] = {0};
    fillBytesPerPixel(sp.colorFormat, bpp);

    size_t hostOffset = 0;
    bool truncated = false;

    for (unsigned int plane = 0; plane < sp.planeParams.num_planes && plane < 4; ++plane) {
        const unsigned int planeWidth = sp.planeParams.width[plane];
        const unsigned int planeHeight = sp.planeParams.height[plane];
        const unsigned int pitch = sp.planeParams.pitch[plane];
        const size_t tightBytes = static_cast<size_t>(planeWidth) * bpp[plane];
        if (tightBytes == 0) {
            std::cerr << "NvVicGraphics2D: Unsupported color format " << sp.colorFormat << std::endl;
            return -1;
        }
        const size_t stride = hostRowStride(plane, tightBytes, callerStride);

        if (NvBufSurfaceMap(surf, 0, plane, NVBUF_MAP_READ_WRITE) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << std::endl;
            return -1;
        }

        uint8_t* dstBase = static_cast<uint8_t*>(surf->surfaceList[0].mappedAddr.addr[plane]);
        for (unsigned int row = 0; row < planeHeight; ++row) {
            const size_t rowOffset = hostOffset + row * stride;
            if (rowOffset + tightBytes > hostSize) {
                truncated = true;
                break;
            }
            memcpy(dstBase + static_cast<size_t>(row) * pitch, hostPtr + rowOffset, tightBytes);
        }

        NvBufSurfaceSyncForDevice(surf, 0, plane);
        NvBufSurfaceUnMap(surf, 0, plane);
        hostOffset += stride * planeHeight;
    }

    if (truncated) {
        std::cerr << "NvVicGraphics2D: host buffer is smaller than the surface, data was truncated" << std::endl;
    }
    return 0;
}

int copySurfaceToHost(NvBufSurface* surf, uint8_t* hostPtr, size_t hostSize, uint32_t callerStride)
{
    if (!hostPtr || !surf || hostSize == 0) {
        return -1;
    }

    const NvBufSurfaceParams& sp = surf->surfaceList[0];
    int bpp[4] = {0};
    fillBytesPerPixel(sp.colorFormat, bpp);

    size_t hostOffset = 0;
    bool truncated = false;

    for (unsigned int plane = 0; plane < sp.planeParams.num_planes && plane < 4; ++plane) {
        const unsigned int planeWidth = sp.planeParams.width[plane];
        const unsigned int planeHeight = sp.planeParams.height[plane];
        const unsigned int pitch = sp.planeParams.pitch[plane];
        const size_t tightBytes = static_cast<size_t>(planeWidth) * bpp[plane];
        if (tightBytes == 0) {
            std::cerr << "NvVicGraphics2D: Unsupported color format " << sp.colorFormat << std::endl;
            return -1;
        }
        const size_t stride = hostRowStride(plane, tightBytes, callerStride);

        if (NvBufSurfaceMap(surf, 0, plane, NVBUF_MAP_READ) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to map plane " << plane << std::endl;
            return -1;
        }
        NvBufSurfaceSyncForCpu(surf, 0, plane);

        const uint8_t* srcBase = static_cast<const uint8_t*>(surf->surfaceList[0].mappedAddr.addr[plane]);
        for (unsigned int row = 0; row < planeHeight; ++row) {
            const size_t rowOffset = hostOffset + row * stride;
            if (rowOffset + tightBytes > hostSize) {
                truncated = true;
                break;
            }
            memcpy(hostPtr + rowOffset, srcBase + static_cast<size_t>(row) * pitch, tightBytes);
        }

        NvBufSurfaceUnMap(surf, 0, plane);
        hostOffset += stride * planeHeight;
    }

    if (truncated) {
        std::cerr << "NvVicGraphics2D: host buffer is smaller than the surface, data was truncated" << std::endl;
    }
    return 0;
}

// ========== CPU access to plane 0 (used by fill / rectangle / blend / swizzle) ==========

class CpuSurfaceMap
{
public:
    CpuSurfaceMap(NvBufSurface* surf, bool write)
        : m_surf(surf)
        , m_write(write)
    {
        if (NvBufSurfaceMap(surf, 0, 0, write ? NVBUF_MAP_READ_WRITE : NVBUF_MAP_READ) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to map surface for CPU access" << std::endl;
            return;
        }
        m_mapped = true;
        NvBufSurfaceSyncForCpu(surf, 0, 0);
        m_data = static_cast<uint8_t*>(surf->surfaceList[0].mappedAddr.addr[0]);
    }

    ~CpuSurfaceMap()
    {
        if (!m_mapped) {
            return;
        }
        if (m_write) {
            NvBufSurfaceSyncForDevice(m_surf, 0, 0);
        }
        NvBufSurfaceUnMap(m_surf, 0, 0);
    }

    CpuSurfaceMap(const CpuSurfaceMap&) = delete;
    CpuSurfaceMap& operator=(const CpuSurfaceMap&) = delete;

    bool valid() const { return m_data != nullptr; }
    uint8_t* row(uint32_t y) const { return m_data + static_cast<size_t>(y) * pitch(); }
    uint32_t pitch() const { return m_surf->surfaceList[0].planeParams.pitch[0]; }

private:
    NvBufSurface* m_surf;
    bool m_write;
    bool m_mapped{false};
    uint8_t* m_data{nullptr};
};

struct Rgba8
{
    uint8_t r, g, b, a;
};

Rgba8 loadPixel(const uint8_t* p, NvBufSurfaceColorFormat format)
{
    if (format == NVBUF_COLOR_FORMAT_BGRA) {
        return {p[2], p[1], p[0], p[3]};
    }
    return {p[0], p[1], p[2], p[3]};
}

void storePixel(uint8_t* p, NvBufSurfaceColorFormat format, Rgba8 c)
{
    if (format == NVBUF_COLOR_FORMAT_BGRA) {
        p[0] = c.b;
        p[1] = c.g;
        p[2] = c.r;
    } else {
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
    }
    p[3] = c.a;
}

/** 0xAARRGGBB -> Rgba8 */
Rgba8 colorFromArgb(uint32_t argb)
{
    return {
        static_cast<uint8_t>((argb >> 16) & 0xff),
        static_cast<uint8_t>((argb >> 8) & 0xff),
        static_cast<uint8_t>(argb & 0xff),
        static_cast<uint8_t>((argb >> 24) & 0xff)};
}

// ========== Rectangles ==========

/**
 * Converts an ImageRect into a rectangle inside a width x height surface.
 * width/height <= 0 selects the whole surface (isExplicit == false).
 */
bool resolveRect(
    const IGraphics2D::ImageRect& rect,
    uint32_t surfWidth,
    uint32_t surfHeight,
    NvBufSurfTransformRect& out,
    bool& isExplicit,
    const char* what)
{
    if (rect.width <= 0 || rect.height <= 0) {
        out = {0, 0, surfWidth, surfHeight};
        isExplicit = false;
        return true;
    }

    if (rect.x < 0 || rect.y < 0 ||
        static_cast<int64_t>(rect.x) + rect.width > surfWidth ||
        static_cast<int64_t>(rect.y) + rect.height > surfHeight) {
        std::cerr << "NvVicGraphics2D: " << what << " rect [" << rect.x << "," << rect.y << ","
                  << rect.width << "," << rect.height << "] is outside the " << surfWidth << "x"
                  << surfHeight << " surface" << std::endl;
        return false;
    }

    out.left = static_cast<uint32_t>(rect.x);
    out.top = static_cast<uint32_t>(rect.y);
    out.width = static_cast<uint32_t>(rect.width);
    out.height = static_cast<uint32_t>(rect.height);
    isExplicit = true;
    return true;
}

bool isOdd(const NvBufSurfTransformRect& r)
{
    return (r.left | r.top | r.width | r.height) & 1u;
}

// ========== VIC transform parameters ==========

NvBufSurfTransform_Inter toVicFilter(IGraphics2D::Interpolation interpolation)
{
    switch (interpolation) {
    case IGraphics2D::Interpolation::Linear:
        return NvBufSurfTransformInter_Bilinear;
    case IGraphics2D::Interpolation::Cubic:
        return NvBufSurfTransformInter_Algo1; // VIC 5-tap
    case IGraphics2D::Interpolation::Average:
        return NvBufSurfTransformInter_Algo2; // VIC 10-tap
    case IGraphics2D::Interpolation::Default:
        break;
    }
    return NvBufSurfTransformInter_Algo4; // VIC "nicest"
}

/**
 * Maps rotation/flip onto the single NvBufSurfTransform_Flip VIC accepts.
 *
 * Direction conventions follow NVIDIA's 07_video_convert sample and
 * gst-nvvideoconvert (the header comments disagree with them):
 *   NvBufSurfTransform_Rotate90  = 90 degrees counter-clockwise
 *   NvBufSurfTransform_FlipX     = horizontal flip (mirror left <-> right)
 *   NvBufSurfTransform_FlipY     = vertical flip   (mirror top  <-> bottom)
 * IGraphics2D::Rotation is clockwise. If a device shows 90/270 or the flips
 * swapped, this is the only place to change.
 *
 * Rotate 90/270 combined with a single-axis mirror needs the Transpose
 * variants, which are not mapped, so that combination is rejected.
 */
bool toVicTransform(
    IGraphics2D::Rotation rotation,
    IGraphics2D::FlipMode flip,
    NvBufSurfTransform_Flip& out,
    bool& active)
{
    using Rotation = IGraphics2D::Rotation;
    using FlipMode = IGraphics2D::FlipMode;

    int clockwiseQuarterTurns = 0;
    switch (rotation) {
    case Rotation::Rotate90:
        clockwiseQuarterTurns = 1;
        break;
    case Rotation::Rotate180:
        clockwiseQuarterTurns = 2;
        break;
    case Rotation::Rotate270:
        clockwiseQuarterTurns = 3;
        break;
    case Rotation::Rotate0:
        break;
    }

    // Mirroring both axes is a 180 degree rotation.
    if (flip == FlipMode::Both) {
        clockwiseQuarterTurns = (clockwiseQuarterTurns + 2) % 4;
        flip = FlipMode::None;
    }

    active = true;
    if (flip == FlipMode::None) {
        switch (clockwiseQuarterTurns) {
        case 0:
            active = false;
            return true;
        case 1:
            out = NvBufSurfTransform_Rotate270; // 90 CW == 270 CCW
            return true;
        case 2:
            out = NvBufSurfTransform_Rotate180;
            return true;
        default:
            out = NvBufSurfTransform_Rotate90; // 270 CW == 90 CCW
            return true;
        }
    }

    if (clockwiseQuarterTurns == 0) {
        out = flip == FlipMode::Horizontal ? NvBufSurfTransform_FlipX : NvBufSurfTransform_FlipY;
        return true;
    }
    if (clockwiseQuarterTurns == 2) {
        // 180 degrees + mirror on one axis == mirror on the other axis.
        out = flip == FlipMode::Horizontal ? NvBufSurfTransform_FlipY : NvBufSurfTransform_FlipX;
        return true;
    }

    active = false;
    return false;
}

NvBufSurfTransform_Error runTransform(
    NvBufSurface* src,
    NvBufSurface* dst,
    NvBufSurfTransformParams& transformParams,
    const char* op)
{
    NvBufSurfTransform_Error ret = NvBufSurfTransform(src, dst, &transformParams);
    if (ret != NvBufSurfTransformError_Success) {
        std::cerr << "NvVicGraphics2D: " << op << " failed with error: " << ret << std::endl;
    }
    return ret;
}

} // namespace

// ========== Construction ==========

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
    std::cout << "NvVicGraphics2D: Destroyed" << std::endl;
}

// ========== Buffer Management ==========

std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> NvVicGraphics2D::createBuffer(
    BufferType type,
    const bsp_perf::bsp_image::ImageView& image)
{
    if (image.desc.width == 0 || image.desc.height == 0) {
        std::cerr << "NvVicGraphics2D: Invalid image size" << std::endl;
        return nullptr;
    }

    const NvBufSurfaceColorFormat colorFormat = mapFormatStringToNvFormat(image.desc.format);
    if (colorFormat == NVBUF_COLOR_FORMAT_INVALID) {
        std::cerr << "NvVicGraphics2D: Invalid color format: " << image.desc.format << std::endl;
        return nullptr;
    }

    if (type == BufferType::Mapped && image.planes[0].data == nullptr) {
        std::cerr << "NvVicGraphics2D: Mapped buffer requires host_ptr" << std::endl;
        return nullptr;
    }

    // Create NvBufSurface with proper parameters
    NvBufSurfaceAllocateParams allocParams{};
    allocParams.params.width = image.desc.widthStride > 0 ? image.desc.widthStride : image.desc.width;
    allocParams.params.height = image.desc.height;
    allocParams.params.layout = NVBUF_LAYOUT_PITCH;
    allocParams.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    allocParams.params.colorFormat = colorFormat;
    allocParams.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

    auto resource = std::make_shared<NvVicSurfaceResource>();
    int ret = NvBufSurfaceAllocate(&resource->surf, 1, &allocParams);
    if (ret != 0 || !resource->surf) {
        resource->surf = nullptr;
        std::cerr << "NvVicGraphics2D: Failed to allocate NvBufSurface: " << ret << std::endl;
        return nullptr;
    }
    resource->surf->numFilled = 1;

    auto g2dBuffer = std::make_shared<impl::G2DBufferInternal>();
    g2dBuffer->g2dPlatform = "nvvic";
    g2dBuffer->bufferType = type;
    g2dBuffer->g2dBufferHandle = resource->surf;
    g2dBuffer->platformData = resource;

    if (type == BufferType::Mapped) {
        const size_t hostSize = image.desc.dataSize > 0 ? image.desc.dataSize : tightHostSize(resource->surf);

        // Copy initial data to NvBufSurface
        if (copyHostToSurface(image.planes[0].data, hostSize, image.planes[0].rowStride, resource->surf) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to copy host data to NvBufSurface" << std::endl;
            return nullptr;
        }
        // Remember host memory for future sync operations
        g2dBuffer->hostPtr = image.planes[0].data;
        g2dBuffer->bufferSize = hostSize;
    }

    auto imageBuffer = std::make_shared<bsp_perf::bsp_image::ImageBuffer>();
    imageBuffer->view = image;
    imageBuffer->nativeHandle = g2dBuffer;
    return imageBuffer;
}

void NvVicGraphics2D::releaseBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer)
{
    if (!buffer) {
        return;
    }
    // Dropping the native handle destroys the NvBufSurface once nothing else uses it.
    buffer->nativeHandle.reset();
}

int NvVicGraphics2D::syncBuffer(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer,
    SyncDirection direction)
{
    auto g2dBuffer = impl::getG2DBufferInternal(buffer);
    if (!g2dBuffer || g2dBuffer->bufferType != BufferType::Mapped) {
        std::cerr << "NvVicGraphics2D: syncBuffer only works with Mapped buffers" << std::endl;
        return -1;
    }

    if (!g2dBuffer->hostPtr || g2dBuffer->bufferSize == 0) {
        std::cerr << "NvVicGraphics2D: Invalid host pointer or buffer size" << std::endl;
        return -1;
    }

    NvBufSurface* surf = surfaceOf(buffer);
    if (!surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface" << std::endl;
        return -1;
    }

    const uint32_t rowStride = buffer->view.planes[0].rowStride;

    if (direction == SyncDirection::CpuToDevice ||
        direction == SyncDirection::Bidirectional) {
        // CPU -> Device: Copy host memory to NvBufSurface
        if (copyHostToSurface(g2dBuffer->hostPtr, g2dBuffer->bufferSize, rowStride, surf) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to sync CPU to Device" << std::endl;
            return -1;
        }
    }

    if (direction == SyncDirection::DeviceToCpu ||
        direction == SyncDirection::Bidirectional) {
        // Device -> CPU: Copy NvBufSurface to host memory
        if (copySurfaceToHost(surf, g2dBuffer->hostPtr, g2dBuffer->bufferSize, rowStride) != 0) {
            std::cerr << "NvVicGraphics2D: Failed to sync Device to CPU" << std::endl;
            return -1;
        }
    }

    return 0;
}

void* NvVicGraphics2D::mapBuffer(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer,
    const std::string& access_mode)
{
    auto g2dBuffer = impl::getG2DBufferInternal(buffer);
    if (!g2dBuffer || g2dBuffer->bufferType != BufferType::Hardware) {
        std::cerr << "NvVicGraphics2D: mapBuffer only works with Hardware buffers" << std::endl;
        return nullptr;
    }

    auto resource = resourceOf(buffer);
    if (!resource || !resource->surf) {
        return nullptr;
    }

    if (resource->mapped) {
        std::cerr << "NvVicGraphics2D: Buffer already mapped" << std::endl;
        return resource->mappedAddr;
    }

    // Determine map flags
    NvBufSurfaceMemMapFlags map_flags = NVBUF_MAP_READ_WRITE;
    if (access_mode == "read") {
        map_flags = NVBUF_MAP_READ;
    } else if (access_mode == "write") {
        map_flags = NVBUF_MAP_WRITE;
    }

    // Map plane 0 (for simplicity, only map first plane)
    if (NvBufSurfaceMap(resource->surf, 0, 0, map_flags) != 0) {
        std::cerr << "NvVicGraphics2D: Failed to map buffer" << std::endl;
        return nullptr;
    }

    resource->mappedAddr = resource->surf->surfaceList[0].mappedAddr.addr[0];
    resource->mapped = true;
    return resource->mappedAddr;
}

void NvVicGraphics2D::unmapBuffer(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> buffer)
{
    auto resource = resourceOf(buffer);
    if (!resource || !resource->surf || !resource->mapped) {
        return;
    }

    NvBufSurfaceUnMap(resource->surf, 0, 0);
    resource->mapped = false;
    resource->mappedAddr = nullptr;
}

bool NvVicGraphics2D::queryCapability(const std::string& capability) const
{
    if (capability == "hardware_draw") return false; // fill/rectangle/blend run on the CPU
    if (capability == "zero_copy_cpu_access") return false;
    if (capability == "requires_explicit_sync") return true;
    if (capability == "rect_blit") return true;
    if (capability == "fill") return true;
    if (capability == "blend") return true;
    if (capability == "rotate") return true;
    if (capability == "flip") return true;
    // "async_job" is not supported: VIC has no job list, createJob() returns nullptr.
    return false;
}

std::string NvVicGraphics2D::getPlatformName() const
{
    return "nvvic";
}

// ========== Image Operations ==========

int NvVicGraphics2D::imageResize(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    return imageBlit(std::move(src), std::move(dst), TransformParams{});
}

int NvVicGraphics2D::imageBlit(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const TransformParams& params)
{
    NvBufSurface* srcSurf = surfaceOf(src);
    NvBufSurface* dstSurf = surfaceOf(dst);
    if (!srcSurf || !dstSurf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for blit" << std::endl;
        return -1;
    }

    NvBufSurfTransform_Flip flip = NvBufSurfTransform_None;
    bool flipActive = false;
    if (!toVicTransform(params.rotation, params.flip, flip, flipActive)) {
        std::cerr << "NvVicGraphics2D: Rotate 90/270 combined with a single-axis flip is not supported" << std::endl;
        return -1;
    }

    NvBufSurfTransformRect srcRect{};
    NvBufSurfTransformRect dstRect{};
    bool srcCrop = false;
    bool dstCrop = false;
    if (!resolveRect(params.srcRect, srcSurf->surfaceList[0].width, srcSurf->surfaceList[0].height, srcRect, srcCrop, "src") ||
        !resolveRect(params.dstRect, dstSurf->surfaceList[0].width, dstSurf->surfaceList[0].height, dstRect, dstCrop, "dst")) {
        return -1;
    }

    NvBufSurfTransformParams transformParams{};
    transformParams.transform_flag = NVBUFSURF_TRANSFORM_FILTER;
    transformParams.transform_filter = toVicFilter(params.interpolation);

    if (srcCrop) {
        transformParams.transform_flag |= NVBUFSURF_TRANSFORM_CROP_SRC;
        transformParams.src_rect = &srcRect;
    }
    if (dstCrop) {
        transformParams.transform_flag |= NVBUFSURF_TRANSFORM_CROP_DST;
        transformParams.dst_rect = &dstRect;
    }
    if ((srcCrop && isOdd(srcRect)) || (dstCrop && isOdd(dstRect))) {
        transformParams.transform_flag |= NVBUFSURF_TRANSFORM_ALLOW_ODD_CROP;
    }
    if (flipActive) {
        transformParams.transform_flag |= NVBUFSURF_TRANSFORM_FLIP;
        transformParams.transform_flip = flip;
    }

    return runTransform(srcSurf, dstSurf, transformParams, "blit") == NvBufSurfTransformError_Success ? 0 : -1;
}

int NvVicGraphics2D::imageCopy(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst)
{
    NvBufSurface* srcSurf = surfaceOf(src);
    NvBufSurface* dstSurf = surfaceOf(dst);

    if (!srcSurf || !dstSurf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for copy" << std::endl;
        return -1;
    }

    int ret = NvBufSurfaceCopy(srcSurf, dstSurf);
    if (ret != 0) {
        std::cerr << "NvVicGraphics2D: Copy failed with error: " << ret << std::endl;
        return -1;
    }
    return 0;
}

int NvVicGraphics2D::imageFill(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    const ImageRect& rect,
    uint32_t color)
{
    NvBufSurface* surf = surfaceOf(dst);
    if (!surf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for fill" << std::endl;
        return -1;
    }

    const NvBufSurfaceParams& sp = surf->surfaceList[0];
    if (!isRgba32(sp.colorFormat)) {
        std::cerr << "NvVicGraphics2D: imageFill only supports RGBA8888/BGRA8888 surfaces" << std::endl;
        return -1;
    }

    NvBufSurfTransformRect region{};
    bool isExplicit = false;
    if (!resolveRect(rect, sp.width, sp.height, region, isExplicit, "fill")) {
        return -1;
    }

    // VIC has no solid fill primitive: write the pixels through a CPU mapping.
    CpuSurfaceMap map(surf, true);
    if (!map.valid()) {
        return -1;
    }

    const Rgba8 rgba = colorFromArgb(color);
    for (uint32_t y = region.top; y < region.top + region.height; ++y) {
        uint8_t* p = map.row(y) + static_cast<size_t>(region.left) * 4;
        for (uint32_t x = 0; x < region.width; ++x, p += 4) {
            storePixel(p, sp.colorFormat, rgba);
        }
    }
    return 0;
}

int NvVicGraphics2D::imageDrawRectangle(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    ImageRect& rect,
    uint32_t color,
    int thickness)
{
    if (rect.width <= 0 || rect.height <= 0) {
        std::cerr << "NvVicGraphics2D: imageDrawRectangle requires a non-empty rect" << std::endl;
        return -1;
    }

    // Draw the outline as four filled strips inside the rectangle.
    const int t = std::max(1, std::min(thickness, std::min(rect.width, rect.height) / 2));
    const ImageRect strips[4] = {
        {rect.x, rect.y, rect.width, t},
        {rect.x, rect.y + rect.height - t, rect.width, t},
        {rect.x, rect.y + t, t, rect.height - 2 * t},
        {rect.x + rect.width - t, rect.y + t, t, rect.height - 2 * t},
    };

    for (const ImageRect& strip : strips) {
        if (strip.width <= 0 || strip.height <= 0) {
            continue;
        }
        if (imageFill(dst, strip, color) != 0) {
            return -1;
        }
    }
    return 0;
}

int NvVicGraphics2D::imageBlend(
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src,
    std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
    BlendMode mode)
{
    NvBufSurface* srcSurf = surfaceOf(src);
    NvBufSurface* dstSurf = surfaceOf(dst);
    if (!srcSurf || !dstSurf || srcSurf == dstSurf) {
        std::cerr << "NvVicGraphics2D: imageBlend needs two distinct valid buffers" << std::endl;
        return -1;
    }

    const NvBufSurfaceParams& sp = srcSurf->surfaceList[0];
    const NvBufSurfaceParams& dp = dstSurf->surfaceList[0];
    if (!isRgba32(sp.colorFormat) || !isRgba32(dp.colorFormat)) {
        std::cerr << "NvVicGraphics2D: imageBlend only supports RGBA8888/BGRA8888 surfaces" << std::endl;
        return -1;
    }
    if (sp.width != dp.width || sp.height != dp.height) {
        std::cerr << "NvVicGraphics2D: imageBlend requires src and dst of the same size" << std::endl;
        return -1;
    }

    // Straight-alpha Porter-Duff on the CPU: VIC blending uses a different model.
    CpuSurfaceMap srcMap(srcSurf, false);
    CpuSurfaceMap dstMap(dstSurf, true);
    if (!srcMap.valid() || !dstMap.valid()) {
        return -1;
    }

    for (uint32_t y = 0; y < dp.height; ++y) {
        const uint8_t* s = srcMap.row(y);
        uint8_t* d = dstMap.row(y);
        for (uint32_t x = 0; x < dp.width; ++x, s += 4, d += 4) {
            const Rgba8 S = loadPixel(s, sp.colorFormat);
            const Rgba8 D = loadPixel(d, dp.colorFormat);
            const float sa = S.a / 255.0F;
            const float da = D.a / 255.0F;

            float fs = 1.0F;
            float fd = 1.0F - sa;
            switch (mode) {
            case BlendMode::SrcOver:
                break;
            case BlendMode::Src:
                fd = 0.0F;
                break;
            case BlendMode::Dst:
                fs = 0.0F;
                fd = 1.0F;
                break;
            case BlendMode::DstOver:
                fs = 1.0F - da;
                fd = 1.0F;
                break;
            }

            const float outA = sa * fs + da * fd;
            Rgba8 out{0, 0, 0, 0};
            if (outA > 0.0F) {
                auto channel = [&](uint8_t sc, uint8_t dc) {
                    const float v = (sc * sa * fs + dc * da * fd) / outA;
                    return static_cast<uint8_t>(std::min(255.0F, std::max(0.0F, v + 0.5F)));
                };
                out.r = channel(S.r, D.r);
                out.g = channel(S.g, D.g);
                out.b = channel(S.b, D.b);
                out.a = static_cast<uint8_t>(std::min(255.0F, outA * 255.0F + 0.5F));
            }
            storePixel(d, dp.colorFormat, out);
        }
    }
    return 0;
}

int NvVicGraphics2D::imageCvtColor(std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> src, std::shared_ptr<bsp_perf::bsp_image::ImageBuffer> dst,
        const std::string& src_format, const std::string& dst_format)
{
    NvBufSurface* srcSurf = surfaceOf(src);
    NvBufSurface* dstSurf = surfaceOf(dst);

    if (!srcSurf || !dstSurf) {
        std::cerr << "NvVicGraphics2D: Failed to get NvBufSurface for color conversion" << std::endl;
        return -1;
    }

    if (mapFormatStringToNvFormat(src_format) == NVBUF_COLOR_FORMAT_INVALID ||
        mapFormatStringToNvFormat(dst_format) == NVBUF_COLOR_FORMAT_INVALID)
    {
        std::cerr << "NvVicGraphics2D: Invalid format for color conversion" << std::endl;
        return -1;
    }

    // The conversion is defined by the formats the two surfaces were created with.
    NvBufSurfTransformRect srcRect = {0, 0, srcSurf->surfaceList[0].width, srcSurf->surfaceList[0].height};
    NvBufSurfTransformRect dstRect = {0, 0, dstSurf->surfaceList[0].width, dstSurf->surfaceList[0].height};

    NvBufSurfTransformParams transformParams{};
    transformParams.transform_flag = NVBUFSURF_TRANSFORM_FILTER;
    transformParams.transform_filter = NvBufSurfTransformInter_Algo4;
    transformParams.src_rect = &srcRect;
    transformParams.dst_rect = &dstRect;

    const NvBufSurfTransform_Error ret = runTransform(srcSurf, dstSurf, transformParams, "color conversion");
    if (ret == NvBufSurfTransformError_Success) {
        return 0;
    }

    // Some VIC firmware lacks BGRA: swap the R/B channels of same-size RGBA<->BGRA on the CPU.
    const NvBufSurfaceParams& sp = srcSurf->surfaceList[0];
    const NvBufSurfaceParams& dp = dstSurf->surfaceList[0];
    const bool swizzlePair = isRgba32(sp.colorFormat) && isRgba32(dp.colorFormat) &&
        sp.width == dp.width && sp.height == dp.height && srcSurf != dstSurf;
    if (ret != NvBufSurfTransformError_Unsupported || !swizzlePair) {
        return -1;
    }

    CpuSurfaceMap srcMap(srcSurf, false);
    CpuSurfaceMap dstMap(dstSurf, true);
    if (!srcMap.valid() || !dstMap.valid()) {
        return -1;
    }
    for (uint32_t y = 0; y < dp.height; ++y) {
        const uint8_t* s = srcMap.row(y);
        uint8_t* d = dstMap.row(y);
        for (uint32_t x = 0; x < dp.width; ++x, s += 4, d += 4) {
            storePixel(d, dp.colorFormat, loadPixel(s, sp.colorFormat));
        }
    }
    return 0;
}
} // namespace bsp_g2d
