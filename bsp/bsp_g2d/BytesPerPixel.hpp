#ifndef __BYTES_PER_PIXEL_HPP__
#define __BYTES_PER_PIXEL_HPP__
#include <unordered_map>
#include <string>
#include <mutex>

namespace bsp_g2d
{

class BytesPerPixel
{
public:
    static BytesPerPixel& getInstance()
    {
        static BytesPerPixel instance;
        return instance;
    }

    float getBytesPerPixel(const std::string& format)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return bytesPerPixelMap.at(format);
    }

private:
    BytesPerPixel() = default;
    ~BytesPerPixel() = default;
    BytesPerPixel(const BytesPerPixel&) = delete;
    BytesPerPixel& operator=(const BytesPerPixel&) = delete;

private:
    std::mutex m_mutex;
    const std::unordered_map<std::string, float> bytesPerPixelMap{
        {"YUV420SP", 1.5},
        {"YUV420P", 1.5},
        {"YUV422SP", 2},
        {"YUV422P", 2},
        {"YUV444SP", 3},
        {"YUV444P", 3},
        {"RGB888", 3},
        {"RGB565", 2},
        {"RGB555", 2},
        {"RGB444", 2},
        {"ARGB8888", 4},
        {"XRGB8888", 4},
        {"ARGB1555", 2},
        {"ARGB4444", 2},
        {"ABGR8888", 4},
        {"XBGR8888", 4},
        {"ABGR5551", 2},
        {"ABGR4444", 2},
        {"RGBA2BPP", 2},
        {"A8", 1},
        {"YCbCr_444_SP", 3},
        {"YCrCb_444_SP", 3},
        {"Y8", 1},
        {"UNKNOWN", 0}
    };
};

} // namespace bsp_g2d

#endif
