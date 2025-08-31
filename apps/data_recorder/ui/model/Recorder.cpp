#include "Recorder.hpp"
#include <shared/ArgParser.hpp>
#include <filesystem>
#include <shared/BspTimeUtils.hpp>

using namespace bsp_perf::shared;
namespace apps
{
namespace data_recorder
{
namespace ui
{

Recorder::Recorder(int argc, char *argv[])
{
    ArgParser parser("Recorder");
    parser.addOption("--encoder, --encoderType", std::string("rkmpp"), "decoder type: rkmpp");
    parser.addOption("--g2d, --graphics2D", std::string("rkrga"), "graphics 2d platform type: rkrga");
    parser.parseArgs(argc, argv);

    std::string encoderType;
    std::string g2dType;
    std::string muxerType;
    parser.getOptionVal("--encoder, --encoderType", encoderType);
    parser.getOptionVal("--g2d, --graphics2D", g2dType);
    parser.getOptionVal("--muxer, --muxerImpl", muxerType);

    m_encoder = IEncoder::create(encoderType);
    m_g2d = IGraphics2D::create(g2dType);
    m_muxer = IMuxer::create(muxerType);
    m_record_dir = std::filesystem::current_path().string();
}

Recorder::~Recorder()
{
    m_encoder.reset();
    m_g2d.reset();
}

int Recorder::startNewRecord()
{
    m_current_filename = "record_" + BspTimeUtils::getCurrentTimeString() + ".mp4";
    IMuxer::MuxConfig muxConfig{true, 30};
    m_muxer->openContainerMux(m_current_filename, muxConfig);
    m_muxer_first_frame = true;
    return 0;
}

int Recorder::stopAndSaveRecord()
{
    m_muxer->endStreamMux();
    m_muxer->closeContainerMux();
    m_muxer_first_frame = true;
    std::cerr << "Recorder::stopAndSaveRecord() record path: " << m_current_filename << std::endl;
    return 0;
}

int Recorder::setupEncoder(int width, int height)
{
    EncodeConfig enc_cfg = {
        .encodingType = "h264",
        .frameFormat = "YUV420SP",
        .fps = 30,
        .width = width,
        .height = height,
        .hor_stride = width,
        .ver_stride = height,
    };

    return m_encoder->setup(enc_cfg);
}

int Recorder::addVideoStream(int width, int height)
{
    StreamInfo streamInfo;
    streamInfo.codec_params.codec_type = "video";
    streamInfo.codec_params.codec_name = "h264";
    streamInfo.codec_params.bit_rate = 1000000;
    streamInfo.codec_params.sample_aspect_ratio = 1.0;
    streamInfo.codec_params.frame_rate = 30;
    streamInfo.codec_params.width = width;
    streamInfo.codec_params.height = height;
    m_stream_packet.stream_index = m_muxer->addStream(streamInfo);
    return 0;
}

int Recorder::convertImageFormat(uint8_t* input_data, int width, int height, std::string input_format,
    std::shared_ptr<EncodeInputBuffer> output_buf, std::string out_format)
{
    IGraphics2D::G2DBufferParams rgb888_g2d_buf_params = {
        .virtual_addr = input_data,
        .rawBufferSize = width * height * 3,
        .width = width,
        .height = height,
        .width_stride = width,
        .height_stride = height,
        .format = input_format,
    };

    auto in_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", rgb888_g2d_buf_params);


    IGraphics2D::G2DBufferParams yuv420_g2d_buf_params = {
        .virtual_addr = output_buf->input_buf_addr,
        .rawBufferSize = width * height * 3 / 2,
        .width = width,
        .height = height,
        .width_stride = width,
        .height_stride = height,
        .format = out_format,
    };

    auto out_g2d_buf = m_g2d->createG2DBuffer("virtualaddr", yuv420_g2d_buf_params);

    m_g2d->imageCvtColor(in_g2d_buf, out_g2d_buf, input_format, out_format);

    return 0;
}

int Recorder::muxerWriteStreamPacket(EncodePacket& enc_pkt)
{
    m_stream_packet.useful_pkt_size = enc_pkt.pkt_len;
    if (m_stream_packet.pkt_data.size() < m_stream_packet.useful_pkt_size)
    {
        m_stream_packet.pkt_data.resize(m_stream_packet.useful_pkt_size);
    }
    std::copy(enc_pkt.encode_pkt.begin(), enc_pkt.encode_pkt.begin() + m_stream_packet.useful_pkt_size,
        m_stream_packet.pkt_data.begin());
    return m_muxer->writeStreamPacket(m_stream_packet);
}

int Recorder::writeRecordFrame(uint8_t* data, int width, int height)
{
    if (m_muxer_first_frame == true)
    {
        setupEncoder(width, height);
        m_enc_in_buf = m_encoder->getInputBuffer();
        addVideoStream(width, height);
        m_muxer_first_frame = false;
    }

    convertImageFormat(data, width, height, "RGB888", m_enc_in_buf, "YUV420SP");

    EncodePacket enc_pkt = {
        .max_size = m_encoder->getFrameSize(),
        .pkt_eos = 0,
        .pkt_len = 0,
    };
    enc_pkt.encode_pkt.resize(enc_pkt.max_size);
    auto encode_len = m_encoder->encode(*m_enc_in_buf, enc_pkt);
    return muxerWriteStreamPacket(enc_pkt);
}
}
}
}