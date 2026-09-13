#include "model/DetectionFrameDecoder.hpp"

#include "model/FramePayload.hpp"

#include <common/msg/ObjDetectMsg.hpp>
#include <msgpack.hpp>
#include <opencv2/opencv.hpp>

#include <algorithm>

namespace apps::data_recorder::ui
{

namespace
{

// Alpha is ignored on 3-channel frames, so one colour serves both formats.
const cv::Scalar kBoxColor{255, 0, 0, 255};
const cv::Scalar kLabelColor{255, 255, 255, 255};
constexpr int kBoxThickness = 2;
constexpr double kLabelScale = 0.4;
constexpr int kLabelBaselineOffset = 12;

void drawDetections(Frame& frame, const apps::data_recorder::ObjDetectMsg& message)
{
    const int cvType = frame.format() == PixelFormat::Rgba8888 ? CV_8UC4 : CV_8UC3;
    cv::Mat canvas(frame.height(), frame.width(), cvType, frame.mutableData(),
        frame.bytesPerLine());

    const std::size_t boxCount = std::min<std::size_t>(message.valid_box_count,
        message.output_boxes.size());

    for (std::size_t i = 0; i < boxCount; ++i)
    {
        const auto& box = message.output_boxes[i];
        cv::rectangle(canvas, cv::Point(box.bbox.left, box.bbox.top),
            cv::Point(box.bbox.right, box.bbox.bottom), kBoxColor, kBoxThickness);
        cv::putText(canvas, box.label,
            cv::Point(box.bbox.left, box.bbox.top + kLabelBaselineOffset),
            cv::FONT_HERSHEY_COMPLEX, kLabelScale, kLabelColor);
    }
}

} // namespace

std::size_t DetectionFrameDecoder::metadataSize() const
{
    return sizeof(apps::data_recorder::ObjDetectMsg);
}

std::shared_ptr<Frame> DetectionFrameDecoder::decode(const uint8_t* metadata,
    std::size_t metadataBytes, midware::zeromq_ipc::SharedMemSubscriber& subscriber,
    FramePool& pool)
{
    msgpack::unpacked unpacked = msgpack::unpack(
        reinterpret_cast<const char*>(metadata), metadataBytes);
    const auto message = unpacked.get().as<apps::data_recorder::ObjDetectMsg>();

    auto frame = readFrameFromSharedMemory(message.original_frame, subscriber, pool);
    if (!frame)
    {
        return nullptr;
    }

    drawDetections(*frame, message);
    return frame;
}

} // namespace apps::data_recorder::ui
