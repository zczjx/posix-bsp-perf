#ifndef APPS_DATA_RECORDER_UI_MODEL_DETECTIONFRAMEDECODER_HPP
#define APPS_DATA_RECORDER_UI_MODEL_DETECTIONFRAMEDECODER_HPP

#include "model/FrameDecoder.hpp"

namespace apps::data_recorder::ui
{

/// Decodes ObjDetectMsg and draws the reported boxes onto the original frame.
class DetectionFrameDecoder final : public FrameDecoder
{
public:
    std::size_t metadataSize() const override;

    std::shared_ptr<Frame> decode(const uint8_t* metadata, std::size_t metadataBytes,
        midware::zeromq_ipc::SharedMemSubscriber& subscriber, FramePool& pool) override;
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_DETECTIONFRAMEDECODER_HPP
