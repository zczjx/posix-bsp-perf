#ifndef APPS_DATA_RECORDER_UI_MODEL_RECORDER_HPP
#define APPS_DATA_RECORDER_UI_MODEL_RECORDER_HPP

#include "core/Frame.hpp"

#include <bsp_codec/IEncoder.hpp>
#include <bsp_container/IMuxer.hpp>
#include <bsp_g2d/IGraphics2D.hpp>

#include <memory>
#include <string>

namespace apps::data_recorder::ui
{

struct RecorderConfig
{
    std::string encoderType;
    std::string g2dType;
    std::string muxerType;
    std::string outputDir;
    int fps{30};
};

/**
 * @brief Encodes submitted frames into an mp4 container.
 *
 * The recorder is deliberately passive: it converts, encodes and muxes whatever
 * frame it is given and never decides when to capture. Encoding happens on the
 * calling thread, which today is the GUI thread.
 */
class Recorder
{
public:
    explicit Recorder(RecorderConfig config);
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    /// Opens a new output file. Returns false and stays idle on failure.
    bool start();

    /// Flushes and closes the current file. Safe to call when idle.
    void stop();

    bool isRecording() const { return m_recording; }

    /// Absolute path of the file opened by the most recent start().
    const std::string& currentPath() const { return m_currentPath; }

    /// Encodes one frame. Returns false if the frame was dropped.
    bool writeFrame(const Frame& frame);

private:
    /// Configures the encoder and the output stream from the first frame's geometry.
    bool configureStream(const Frame& frame);

    bool convertToEncoderInput(const Frame& frame, bsp_perf::bsp_image::ImageBuffer& target);

    bool writePacket(const bsp_codec::EncodePacket& packet);

    RecorderConfig m_config;
    std::unique_ptr<bsp_codec::IEncoder> m_encoder;
    std::unique_ptr<bsp_g2d::IGraphics2D> m_g2d;
    std::unique_ptr<bsp_container::IMuxer> m_muxer;
    bsp_container::StreamPacket m_streamPacket{};
    std::string m_currentPath;
    bool m_recording{false};
    bool m_streamConfigured{false};
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_MODEL_RECORDER_HPP
