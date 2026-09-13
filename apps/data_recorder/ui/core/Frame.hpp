#ifndef APPS_DATA_RECORDER_UI_CORE_FRAME_HPP
#define APPS_DATA_RECORDER_UI_CORE_FRAME_HPP

#include <QMetaType>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace apps::data_recorder::ui
{

enum class PixelFormat
{
    Unknown,
    Rgb888,
    Rgba8888,
};

PixelFormat pixelFormatFromString(const std::string& name);
const char* toString(PixelFormat format);
int bytesPerPixel(PixelFormat format);

/**
 * @brief A self-owning, tightly packed image.
 *
 * Frames are published as FramePtr (a shared_ptr to const) so a producer thread
 * can hand pixels to the GUI thread through a queued signal/slot connection
 * without the consumer racing against the next capture. Pixel rows carry no
 * padding, so bytesPerLine() is always width * bytesPerPixel(format).
 */
class Frame
{
public:
    Frame() = default;
    Frame(int width, int height, PixelFormat format);

    int width() const { return m_width; }
    int height() const { return m_height; }
    PixelFormat format() const { return m_format; }

    std::size_t bytesPerLine() const;
    std::size_t sizeBytes() const { return m_pixels.size(); }
    bool empty() const { return m_pixels.empty(); }

    const uint8_t* data() const { return m_pixels.data(); }

    /// Writable pixels. Only the producer holding the sole reference may write,
    /// which in practice means before the frame is published as a FramePtr.
    uint8_t* mutableData() { return m_pixels.data(); }

    /// Re-describes the frame in place, keeping the existing allocation when the
    /// byte count is unchanged.
    void reset(int width, int height, PixelFormat format);

private:
    int m_width{0};
    int m_height{0};
    PixelFormat m_format{PixelFormat::Unknown};
    std::vector<uint8_t> m_pixels;
};

using FramePtr = std::shared_ptr<const Frame>;

/**
 * @brief Recycles Frame allocations so the steady-state capture path is allocation free.
 *
 * A retained frame is handed out again only when the pool holds the last
 * reference to it, so a frame still queued for the GUI or the recorder is never
 * overwritten. This is only sound because the pool is the sole creator of
 * references: consumers may drop a frame from any thread, but never add one.
 * Each pool belongs to a single producer thread and is not itself thread safe.
 */
class FramePool
{
public:
    explicit FramePool(std::size_t capacity = 4);

    std::shared_ptr<Frame> acquire(int width, int height, PixelFormat format);

private:
    std::vector<std::shared_ptr<Frame>> m_retained;
    std::size_t m_capacity;
};

} // namespace apps::data_recorder::ui

Q_DECLARE_METATYPE(apps::data_recorder::ui::FramePtr)

#endif // APPS_DATA_RECORDER_UI_CORE_FRAME_HPP
