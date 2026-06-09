#ifndef __BSP_IMAGE_FORMAT_HPP__
#define __BSP_IMAGE_FORMAT_HPP__

#include "ImageTypes.hpp"
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace bsp_perf
{
namespace bsp_image
{

class ImageFormat
{
public:
    static ImageFormat& getInstance()
    {
        static ImageFormat instance;
        return instance;
    }

    float bytesPerPixel(const std::string& format)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_bytesPerPixelMap.at(format);
    }

    size_t imageDataSize(const ImageDesc& desc)
    {
        const uint32_t widthStride = desc.widthStride > 0 ? desc.widthStride : desc.width;
        const uint32_t heightStride = desc.heightStride > 0 ? desc.heightStride : desc.height;
        const size_t pixelCount = static_cast<size_t>(widthStride) * static_cast<size_t>(heightStride);
        return static_cast<size_t>(static_cast<float>(pixelCount) * bytesPerPixel(desc.format));
    }

private:
    ImageFormat() = default;
    ~ImageFormat() = default;
    ImageFormat(const ImageFormat&) = delete;
    ImageFormat& operator=(const ImageFormat&) = delete;

private:
    std::mutex m_mutex;
    const std::unordered_map<std::string, float> m_bytesPerPixelMap{
        {"YUV420SP", 1.5F},
        {"YUV420P", 1.5F},
        {"YUV422SP", 2.0F},
        {"YUV422P", 2.0F},
        {"YUV444SP", 3.0F},
        {"YUV444P", 3.0F},
        {"YUV420SP_10BIT", 2.0F},
        {"YUV420P_10BIT", 2.0F},
        {"YUV422SP_10BIT", 3.0F},
        {"YUV422P_10BIT", 3.0F},
        {"YUV444SP_10BIT", 4.0F},
        {"YUV444P_10BIT", 4.0F},
        {"RGB888", 3.0F},
        {"BGR888", 3.0F},
        {"RGB565", 2.0F},
        {"RGB555", 2.0F},
        {"RGB444", 2.0F},
        {"ARGB8888", 4.0F},
        {"XRGB8888", 4.0F},
        {"ARGB1555", 2.0F},
        {"ARGB4444", 2.0F},
        {"ABGR8888", 4.0F},
        {"XBGR8888", 4.0F},
        {"BGRA8888", 4.0F},
        {"RGBA8888", 4.0F},
        {"ABGR5551", 2.0F},
        {"ABGR4444", 2.0F},
        {"RGBA2BPP", 2.0F},
        {"A8", 1.0F},
        {"YCbCr_444_SP", 3.0F},
        {"YCrCb_444_SP", 3.0F},
        {"Y8", 1.0F},
        {"UNKNOWN", 0.0F},
    };
};

inline float bytesPerPixel(const std::string& format)
{
    return ImageFormat::getInstance().bytesPerPixel(format);
}

inline size_t imageDataSize(const ImageDesc& desc)
{
    return ImageFormat::getInstance().imageDataSize(desc);
}

} // namespace bsp_image
} // namespace bsp_perf

#endif // __BSP_IMAGE_FORMAT_HPP__
