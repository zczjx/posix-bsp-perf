#include "model/FramePayload.hpp"

#include "core/Logging.hpp"

#include <algorithm>

namespace apps::data_recorder::ui
{

std::shared_ptr<Frame> readFrameFromSharedMemory(const apps::data_recorder::CameraSensorMsg& meta,
    midware::zeromq_ipc::SharedMemSubscriber& subscriber, FramePool& pool)
{
    const PixelFormat format = pixelFormatFromString(meta.pixel_format);
    if (format == PixelFormat::Unknown)
    {
        qCWarning(uiLog, "Dropping frame from '%s': unsupported pixel format '%s'",
            meta.publisher_id.c_str(), meta.pixel_format.c_str());
        return nullptr;
    }

    auto frame = pool.acquire(meta.width, meta.height, format);
    if (frame->empty())
    {
        qCWarning(uiLog, "Dropping frame from '%s': invalid geometry %dx%d",
            meta.publisher_id.c_str(), meta.width, meta.height);
        return nullptr;
    }

    const std::size_t copyBytes = std::min(meta.data_size, frame->sizeBytes());
    if (copyBytes < frame->sizeBytes())
    {
        qCWarning(uiLog, "Frame from '%s' carries %zu bytes but %dx%d %s needs %zu",
            meta.publisher_id.c_str(), meta.data_size, meta.width, meta.height,
            toString(format), frame->sizeBytes());
    }

    if (subscriber.receiveSharedMemData(frame->mutableData(), copyBytes, meta.slot_index) < 0)
    {
        return nullptr;
    }

    return frame;
}

} // namespace apps::data_recorder::ui
