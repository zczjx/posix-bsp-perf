#ifndef RECORDER_HPP
#define RECORDER_HPP

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <bsp_codec/IEncoder.hpp>
#include <bsp_g2d/IGraphics2D.hpp>
#include <bsp_container/IMuxer.hpp>

using namespace bsp_codec;
using namespace bsp_g2d;
using namespace bsp_container;

namespace apps
{
namespace data_recorder
{
namespace ui
{

class Recorder
{
public:
    Recorder(int argc, char *argv[]);
    ~Recorder();

    int startNewRecord();

    int stopAndSaveRecord();

    int writeRecordFrame(uint8_t* data, int width, int height, std::string format = "RGB888");

    std::string getRecordPath() const
    {
        return m_record_dir + "/" + m_current_filename;
    }


private:
    int setupEncoder(int width, int height);

    int addVideoStream(int width, int height);

    int convertImageFormat(uint8_t* input_data, int width, int height, std::string input_format,
            std::shared_ptr<EncodeInputBuffer> output_buf, std::string out_format);

    int muxerWriteStreamPacket(EncodePacket& enc_pkt);


private:
    std::string m_record_dir;;
    std::string m_current_filename;
    std::unique_ptr<IEncoder> m_encoder{nullptr};
    std::shared_ptr<EncodeInputBuffer> m_enc_in_buf{nullptr};
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::unique_ptr<IMuxer> m_muxer{nullptr};
    StreamPacket m_stream_packet{};
    std::atomic<bool> m_muxer_first_frame{true};

};

} // namespace ui
} // namespace data_recorder
} // namespace apps
#endif // RECORDER_HPP