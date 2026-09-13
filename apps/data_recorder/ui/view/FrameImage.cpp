#include "view/FrameImage.hpp"

#include <cstring>

namespace apps::data_recorder::ui
{

namespace
{

QImage::Format toQImageFormat(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::Rgb888:
        return QImage::Format_RGB888;
    case PixelFormat::Rgba8888:
        return QImage::Format_RGBA8888;
    case PixelFormat::Unknown:
        break;
    }

    return QImage::Format_Invalid;
}

} // namespace

QImage asImageView(const Frame& frame)
{
    const QImage::Format qtFormat = toQImageFormat(frame.format());
    if (frame.empty() || qtFormat == QImage::Format_Invalid)
    {
        return {};
    }

    return QImage(frame.data(), frame.width(), frame.height(),
        static_cast<int>(frame.bytesPerLine()), qtFormat);
}

std::shared_ptr<Frame> frameFromImage(const QImage& image, PixelFormat format, FramePool& pool)
{
    const QImage::Format qtFormat = toQImageFormat(format);
    if (image.isNull() || qtFormat == QImage::Format_Invalid)
    {
        return nullptr;
    }

    const QImage converted = image.format() == qtFormat ? image : image.convertToFormat(qtFormat);
    if (converted.isNull())
    {
        return nullptr;
    }

    auto frame = pool.acquire(converted.width(), converted.height(), format);
    if (frame->empty())
    {
        return nullptr;
    }

    const std::size_t lineBytes = frame->bytesPerLine();
    uint8_t* target = frame->mutableData();

    for (int row = 0; row < converted.height(); ++row)
    {
        std::memcpy(target + static_cast<std::size_t>(row) * lineBytes,
            converted.constScanLine(row), lineBytes);
    }

    return frame;
}

} // namespace apps::data_recorder::ui
