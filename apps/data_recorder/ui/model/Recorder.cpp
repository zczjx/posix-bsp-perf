#include "Recorder.hpp"
#include <bsp_g2d/BufferHelper.hpp>
#include <shared/ArgParser.hpp>
#include <filesystem>
#include <shared/BspTimeUtils.hpp>
#include <cstring>

using namespace bsp_perf::shared;
namespace apps
{
namespace data_recorder
{
namespace ui
{

namespace
{
int alignUp(int value, int alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

int bytesPerPixel(const std::string& format)
{
    if (format == "RGBA8888" || format == "ARGB8888" || format == "BGRA8888")
    {
        return 4;
    }
    return 3;
}

uint8_t clampToByte(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 255)
    {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

int convertToYuv420spCpu(uint8_t* input_data, int width, int height, const std::string& input_format,
    uint8_t* output_data, int output_stride, int output_height_stride)
{
    if (input_data == nullptr || output_data == nullptr)
    {
        return -1;
    }

    const int input_bpp = bytesPerPixel(input_format);
    uint8_t* y_plane = output_data;
    uint8_t* uv_plane = output_data + output_stride * output_height_stride;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const uint8_t* pixel = input_data + (static_cast<size_t>(y) * width + x) * input_bpp;
            const int r = pixel[0];
            const int g = pixel[1];
            const int b = pixel[2];
            y_plane[y * output_stride + x] = clampToByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
    }

    for (int y = 0; y < height; y += 2)
    {
        for (int x = 0; x < width; x += 2)
        {
            const uint8_t* pixel = input_data + (static_cast<size_t>(y) * width + x) * input_bpp;
            const int r = pixel[0];
            const int g = pixel[1];
            const int b = pixel[2];
            const uint8_t u = clampToByte(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            const uint8_t v = clampToByte(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            const size_t uv_index = static_cast<size_t>(y / 2) * output_stride + x;
            uv_plane[uv_index] = u;
            uv_plane[uv_index + 1] = v;
        }
    }

    return 0;
}
}

Recorder::Recorder(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++)
    {
        std::cout << "arg: " << argv[i] << std::endl;
    }
    ArgParser parser("Recorder");
    parser.addOption("--encoder", std::string("rkmpp"), "decoder type: rkmpp");
    parser.addOption("--g2d", std::string("rkrga"), "graphics 2d platform type: rkrga");
    parser.addOption("--muxer", std::string("FFmpegMuxer"), "Muxer Impl: FFmpegMuxer");
    parser.parseArgs(argc, argv);

    std::string encoderType;
    std::string g2dType;
    std::string muxerType;
    parser.getOptionVal("--encoder", encoderType);
    parser.getOptionVal("--g2d", g2dType);
    parser.getOptionVal("--muxer", muxerType);

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
    std::cout << "Recorder::startNewRecord()" << std::endl;
    m_current_filename = "record_" + BspTimeUtils::getCurrentTimeString() + ".mp4";
    IMuxer::MuxConfig muxConfig{true, 30};
    m_muxer->openContainerMux(m_current_filename, muxConfig);
    m_muxer_first_frame = true;
    return 0;
}

int Recorder::stopAndSaveRecord()
{
    m_encoder->tearDown();
    m_muxer->endStreamMux();
    m_muxer->closeContainerMux();
    m_muxer_first_frame = true;
    std::cerr << "Recorder::stopAndSaveRecord() record path: " << m_current_filename << std::endl;
    return 0;
}

int Recorder::setupEncoder(int width, int height)
{
    m_enc_width = width;
    m_enc_height = height;
    m_enc_hor_stride = alignUp(width, 16);
    m_enc_ver_stride = alignUp(height, 16);

    EncodeConfig enc_cfg = {
        .encodingType = "h264",
        .frameFormat = "YUV420SP",
        .fps = 30,
        .width = static_cast<uint32_t>(m_enc_width),
        .height = static_cast<uint32_t>(m_enc_height),
        .hor_stride = static_cast<uint32_t>(m_enc_hor_stride),
        .ver_stride = static_cast<uint32_t>(m_enc_ver_stride),
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
        .width = static_cast<size_t>(width),
        .height = static_cast<size_t>(height),
        .width_stride = static_cast<size_t>(width),
        .height_stride = static_cast<size_t>(height),
        .format = input_format,
        .host_ptr = input_data,
        .buffer_size = static_cast<size_t>(width) * height * bytesPerPixel(input_format),
    };

    auto in_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, rgb888_g2d_buf_params);


    IGraphics2D::G2DBufferParams yuv420_g2d_buf_params = {
        .width = static_cast<size_t>(width),
        .height = static_cast<size_t>(height),
        .width_stride = static_cast<size_t>(m_enc_hor_stride > 0 ? m_enc_hor_stride : alignUp(width, 16)),
        .height_stride = static_cast<size_t>(m_enc_ver_stride > 0 ? m_enc_ver_stride : alignUp(height, 16)),
        .format = out_format,
        .host_ptr = output_buf->input_buf_addr,
        .buffer_size = static_cast<size_t>(m_enc_hor_stride > 0 ? m_enc_hor_stride : alignUp(width, 16)) *
            static_cast<size_t>(m_enc_ver_stride > 0 ? m_enc_ver_stride : alignUp(height, 16)) * 3 / 2,
    };

    std::memset(output_buf->input_buf_addr, 0, yuv420_g2d_buf_params.buffer_size);

    auto out_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, yuv420_g2d_buf_params);

    int ret = -1;
    if (in_g2d_buf && out_g2d_buf)
    {
        ret = m_g2d->imageCvtColor(in_g2d_buf, out_g2d_buf, input_format, out_format);
        if (ret == 0)
        {
            bsp_g2d::BufferSyncGuard sync(
                m_g2d.get(),
                out_g2d_buf,
                IGraphics2D::SyncDirection::Bidirectional);
        }
    }

    if (in_g2d_buf)
    {
        m_g2d->releaseBuffer(in_g2d_buf);
    }
    if (out_g2d_buf)
    {
        m_g2d->releaseBuffer(out_g2d_buf);
    }

    if (ret != 0)
    {
        std::cerr << "Recorder::convertImageFormat() RGA convert failed, fallback to CPU conversion" << std::endl;
        return convertToYuv420spCpu(input_data, width, height, input_format,
            static_cast<uint8_t*>(output_buf->input_buf_addr),
            static_cast<int>(yuv420_g2d_buf_params.width_stride),
            static_cast<int>(yuv420_g2d_buf_params.height_stride));
    }

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

int Recorder::writeRecordFrame(uint8_t* data, int width, int height, std::string format)
{
    if (m_muxer_first_frame == true)
    {
        setupEncoder(width, height);
        m_enc_in_buf = m_encoder->getInputBuffer();
        addVideoStream(width, height);
        m_muxer_first_frame = false;
    }

    if (convertImageFormat(data, width, height, format, m_enc_in_buf, "YUV420SP") != 0)
    {
        std::cerr << "Recorder::writeRecordFrame() convert image format failed" << std::endl;
        return -1;
    }

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