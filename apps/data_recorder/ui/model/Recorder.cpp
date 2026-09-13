#include "model/Recorder.hpp"

#include "core/Logging.hpp"

#include <shared/BspTimeUtils.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace apps::data_recorder::ui
{

namespace
{

constexpr const char* kEncodingType = "h264";
constexpr const char* kEncoderFrameFormat = "YUV420SP";
constexpr int64_t kBitRate = 1000000;

} // namespace

Recorder::Recorder(RecorderConfig config)
    : m_config(std::move(config))
{
    m_encoder = bsp_codec::IEncoder::create(m_config.encoderType);
    m_g2d = bsp_g2d::IGraphics2D::create(m_config.g2dType);
    m_muxer = bsp_container::IMuxer::create(m_config.muxerType);

    if (m_config.outputDir.empty())
    {
        m_config.outputDir = std::filesystem::current_path().string();
    }
}

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start()
{
    if (m_recording)
    {
        return true;
    }

    const std::string filename =
        "record_" + bsp_perf::shared::BspTimeUtils::getCurrentTimeString() + ".mp4";
    const std::string path = (std::filesystem::path(m_config.outputDir) / filename).string();

    bsp_container::IMuxer::MuxConfig muxConfig{};
    muxConfig.ts_recreate = true;
    muxConfig.video_fps = static_cast<float>(m_config.fps);

    if (m_muxer->openContainerMux(path, muxConfig) != 0)
    {
        qCWarning(uiLog, "Cannot open '%s' for recording", path.c_str());
        return false;
    }

    m_currentPath = path;
    m_recording = true;
    m_streamConfigured = false;

    qCInfo(uiLog, "Recording to %s", m_currentPath.c_str());
    return true;
}

void Recorder::stop()
{
    if (!m_recording)
    {
        return;
    }

    // Only tear the encoder down if a frame ever configured it.
    if (m_streamConfigured)
    {
        m_encoder->tearDown();
    }

    m_muxer->endStreamMux();
    m_muxer->closeContainerMux();

    m_recording = false;
    m_streamConfigured = false;

    qCInfo(uiLog, "Recording saved to %s", m_currentPath.c_str());
}

bool Recorder::configureStream(const Frame& frame)
{
    bsp_codec::EncodeConfig encodeConfig{};
    encodeConfig.encodingType = kEncodingType;
    encodeConfig.frameFormat = kEncoderFrameFormat;
    encodeConfig.fps = m_config.fps;
    encodeConfig.width = static_cast<uint32_t>(frame.width());
    encodeConfig.height = static_cast<uint32_t>(frame.height());
    encodeConfig.hor_stride = static_cast<uint32_t>(frame.width());
    encodeConfig.ver_stride = static_cast<uint32_t>(frame.height());

    if (m_encoder->setup(encodeConfig) != 0)
    {
        qCWarning(uiLog, "Encoder setup failed for %dx%d", frame.width(), frame.height());
        return false;
    }

    bsp_container::StreamInfo streamInfo{};
    streamInfo.codec_params.codec_type = "video";
    streamInfo.codec_params.codec_name = kEncodingType;
    streamInfo.codec_params.bit_rate = kBitRate;
    streamInfo.codec_params.sample_aspect_ratio = 1.0F;
    streamInfo.codec_params.frame_rate = m_config.fps;
    streamInfo.codec_params.width = frame.width();
    streamInfo.codec_params.height = frame.height();

    const int streamIndex = m_muxer->addStream(streamInfo);
    if (streamIndex < 0)
    {
        qCWarning(uiLog, "Cannot add a video stream to the container");
        return false;
    }

    m_streamPacket.stream_index = streamIndex;
    return true;
}

bool Recorder::convertToEncoderInput(const Frame& frame, bsp_perf::bsp_image::ImageBuffer& target)
{
    bsp_perf::bsp_image::ImageDesc sourceDesc{};
    sourceDesc.width = static_cast<uint32_t>(frame.width());
    sourceDesc.height = static_cast<uint32_t>(frame.height());
    sourceDesc.widthStride = static_cast<uint32_t>(frame.width());
    sourceDesc.heightStride = static_cast<uint32_t>(frame.height());
    sourceDesc.format = toString(frame.format());
    sourceDesc.dataSize = frame.sizeBytes();

    // The g2d interface takes a writable view; the source is only ever read.
    const auto sourceView = bsp_perf::bsp_image::makeHostImageView(
        const_cast<uint8_t*>(frame.data()), sourceDesc, static_cast<uint32_t>(frame.width()),
        bsp_perf::bsp_image::ImageAccess::ReadOnly);

    target.view.desc.format = kEncoderFrameFormat;
    return m_g2d->imageCvtColorToHost(sourceView, target.view) == 0;
}

bool Recorder::writePacket(const bsp_codec::EncodePacket& packet)
{
    m_streamPacket.useful_pkt_size = packet.pkt_len;
    if (m_streamPacket.pkt_data.size() < packet.pkt_len)
    {
        m_streamPacket.pkt_data.resize(packet.pkt_len);
    }

    std::copy(packet.encode_pkt.begin(), packet.encode_pkt.begin() + packet.pkt_len,
        m_streamPacket.pkt_data.begin());

    return m_muxer->writeStreamPacket(m_streamPacket) == 0;
}

bool Recorder::writeFrame(const Frame& frame)
{
    if (!m_recording || frame.empty())
    {
        return false;
    }

    if (!m_streamConfigured)
    {
        if (!configureStream(frame))
        {
            return false;
        }

        m_streamConfigured = true;
    }

    // Acquired per frame: the encoder returns the buffer to its pool inside encode().
    auto inputBuffer = m_encoder->getInputBuffer();
    if (!inputBuffer)
    {
        qCWarning(uiLog, "Dropping frame: encoder input pool exhausted");
        return false;
    }

    if (!convertToEncoderInput(frame, *inputBuffer))
    {
        qCWarning(uiLog, "Dropping frame: %s to %s conversion failed",
            toString(frame.format()), kEncoderFrameFormat);
        return false;
    }

    bsp_codec::EncodePacket packet{};
    packet.max_size = m_encoder->getFrameSize();
    packet.pkt_eos = 0;
    packet.pkt_len = 0;
    packet.encode_pkt.resize(packet.max_size);

    // Only negative means failure: nvenc returns 0 on success while rkmpp
    // returns the encoded byte count.
    if (m_encoder->encode(*inputBuffer, packet) < 0)
    {
        qCWarning(uiLog, "Dropping frame: encode failed");
        return false;
    }

    if (packet.pkt_len == 0)
    {
        // Expected while a hardware encoder fills its pipeline.
        return true;
    }

    return writePacket(packet);
}

} // namespace apps::data_recorder::ui
