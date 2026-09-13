#include "model/CameraFrameDecoder.hpp"

#include "model/FramePayload.hpp"

#include <common/msg/CameraSensorMsg.hpp>
#include <msgpack.hpp>

namespace apps::data_recorder::ui
{

std::size_t CameraFrameDecoder::metadataSize() const
{
    return sizeof(apps::data_recorder::CameraSensorMsg);
}

std::shared_ptr<Frame> CameraFrameDecoder::decode(const uint8_t* metadata,
    std::size_t metadataBytes, midware::zeromq_ipc::SharedMemSubscriber& subscriber,
    FramePool& pool)
{
    msgpack::unpacked unpacked = msgpack::unpack(
        reinterpret_cast<const char*>(metadata), metadataBytes);
    const auto meta = unpacked.get().as<apps::data_recorder::CameraSensorMsg>();

    return readFrameFromSharedMemory(meta, subscriber, pool);
}

} // namespace apps::data_recorder::ui
