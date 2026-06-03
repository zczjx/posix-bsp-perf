#ifndef __BYTES_PER_PIXEL_HPP__
#define __BYTES_PER_PIXEL_HPP__

#include <image/ImageFormat.hpp>
#include <string>

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
        return bsp_perf::image::bytesPerPixel(format);
    }

private:
    BytesPerPixel() = default;
    ~BytesPerPixel() = default;
    BytesPerPixel(const BytesPerPixel&) = delete;
    BytesPerPixel& operator=(const BytesPerPixel&) = delete;

private:
};

} // namespace bsp_g2d

#endif
