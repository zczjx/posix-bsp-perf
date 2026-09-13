#ifndef APPS_DATA_RECORDER_UI_MODEL_FRAMEPAYLOAD_HPP
#define APPS_DATA_RECORDER_UI_MODEL_FRAMEPAYLOAD_HPP

#include "core/Frame.hpp"

#include <common/msg/CameraSensorMsg.hpp>
#include <zeromq_ipc/sharedMemSubscriber.hpp>

#include <memory>

namespace apps::data_recorder::ui
{

/**
 * @brief Copies the shared-memory payload described by @p meta into a pooled frame.
 *
 * The frame is sized from the message geometry rather than from whatever the
 * first message happened to declare, and the copy is clamped to the frame, so a
 * publisher that changes resolution mid-stream cannot overrun the buffer.
 *
 * @return the filled frame, or nullptr when the format is unsupported or the
 *         geometry is unusable.
 */
std::shared_ptr<Frame> readFrameFromSharedMemory(const apps::data_recorder::CameraSensorMsg& meta,
    midware::zeromq_ipc::SharedMemSubscriber& subscriber, FramePool& pool);

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_FRAMEPAYLOAD_HPP
