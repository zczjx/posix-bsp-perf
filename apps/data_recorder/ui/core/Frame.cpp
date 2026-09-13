#include "Frame.hpp"

namespace apps::data_recorder::ui
{

PixelFormat pixelFormatFromString(const std::string& name)
{
    if (name == "RGB888")
    {
        return PixelFormat::Rgb888;
    }

    if (name == "RGBA8888")
    {
        return PixelFormat::Rgba8888;
    }

    return PixelFormat::Unknown;
}

const char* toString(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::Rgb888:
        return "RGB888";
    case PixelFormat::Rgba8888:
        return "RGBA8888";
    case PixelFormat::Unknown:
        break;
    }

    return "UNKNOWN";
}

int bytesPerPixel(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::Rgb888:
        return 3;
    case PixelFormat::Rgba8888:
        return 4;
    case PixelFormat::Unknown:
        break;
    }

    return 0;
}

Frame::Frame(int width, int height, PixelFormat format)
{
    reset(width, height, format);
}

std::size_t Frame::bytesPerLine() const
{
    return static_cast<std::size_t>(m_width) * bytesPerPixel(m_format);
}

void Frame::reset(int width, int height, PixelFormat format)
{
    if (width <= 0 || height <= 0 || format == PixelFormat::Unknown)
    {
        m_width = 0;
        m_height = 0;
        m_format = PixelFormat::Unknown;
        m_pixels.clear();
        return;
    }

    m_width = width;
    m_height = height;
    m_format = format;
    m_pixels.resize(static_cast<std::size_t>(width) * height * bytesPerPixel(format));
}

FramePool::FramePool(std::size_t capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
{
}

std::shared_ptr<Frame> FramePool::acquire(int width, int height, PixelFormat format)
{
    for (auto& frame: m_retained)
    {
        if (frame.use_count() == 1)
        {
            frame->reset(width, height, format);
            return frame;
        }
    }

    auto frame = std::make_shared<Frame>(width, height, format);
    if (m_retained.size() < m_capacity)
    {
        m_retained.push_back(frame);
    }

    return frame;
}

} // namespace apps::data_recorder::ui
