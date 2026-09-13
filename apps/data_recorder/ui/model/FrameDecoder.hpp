#ifndef APPS_DATA_RECORDER_UI_MODEL_FRAMEDECODER_HPP
#define APPS_DATA_RECORDER_UI_MODEL_FRAMEDECODER_HPP

#include "core/Frame.hpp"

#include <zeromq_ipc/sharedMemSubscriber.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace apps::data_recorder::ui
{

/**
 * @brief Turns one published message into a displayable frame.
 *
 * ShmFrameSource owns the subscription, the worker thread and the frame pool;
 * a decoder only knows how to interpret the bytes of one message type. Adding a
 * new producer therefore means adding a decoder, not another consumer loop.
 */
class FrameDecoder
{
public:
    virtual ~FrameDecoder() = default;

    /// Size of the buffer to hand to receiveMsg() for this message type.
    virtual std::size_t metadataSize() const = 0;

    /**
     * @param metadata     msgpack-encoded message header, @p metadataBytes long.
     * @param subscriber   used to pull the pixel payload out of shared memory.
     * @param pool         provides the frame to fill.
     * @return the decoded frame, or nullptr when the message should be dropped.
     * @throws may propagate msgpack errors; the caller treats them as a dropped frame.
     */
    virtual std::shared_ptr<Frame> decode(const uint8_t* metadata, std::size_t metadataBytes,
        midware::zeromq_ipc::SharedMemSubscriber& subscriber, FramePool& pool) = 0;

protected:
    FrameDecoder() = default;
    FrameDecoder(const FrameDecoder&) = delete;
    FrameDecoder& operator=(const FrameDecoder&) = delete;
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_FRAMEDECODER_HPP
