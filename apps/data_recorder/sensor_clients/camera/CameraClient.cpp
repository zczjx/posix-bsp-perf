#include "CameraClient.hpp"
#include <iostream>
namespace apps
{
namespace data_recorder
{

CameraClient::CameraClient(const json& sensor_context, const json& vehicle_info)
    : SensorClient(sensor_context)
{
    m_xres = sensor_context["image_size_x"];
    m_yres = sensor_context["image_size_y"];
    m_raw_pixel_format = sensor_context["raw_pixel_format"];
    m_video_dec_helper = std::make_unique<VideoDecHelper>(vehicle_info["target_platform"]);
    DecodeConfig cfg = {
        .encoding = "h264",
        .fps = 30,
    };
    m_video_dec_helper->setupAndStartDecoder(cfg);
    m_main_thread = std::make_unique<std::thread>([this]() {runLoop();});
    m_consumer_thread = std::make_unique<std::thread>([this]() {consumerLoop();});
}

CameraClient::~CameraClient()
{
    if (m_main_thread->joinable() || m_consumer_thread->joinable())
    {
        m_stopSignal.store(true);
        m_consumer_thread->join();
        m_main_thread->join();
    }
}

std::shared_ptr<DecodeOutFrame> CameraClient::getCameraVideoFrame()
{
    return m_video_dec_helper->getDecodedFrame();
}

void CameraClient::consumerLoop()
{
    size_t frame_count = 0;
    while (!m_stopSignal.load())
    {
        std::shared_ptr<DecodeOutFrame> frame = getCameraVideoFrame();
        if (frame != nullptr)
        {
            frame_count++;
            std::cout << "CameraClient::consumerLoop() get a frame, frame_count: " << frame_count << std::endl;
            frame.reset();
        }
    }
    std::cout << "CameraClient::consumerLoop() end, frame_count: " << frame_count << std::endl;
}

void CameraClient::runLoop()
{
    uint64_t frame_count = 0;
    std::cout << "runLoop" << std::endl;
    size_t min_buffer_size = 1024 * 1024;
    RtpHeader header;

    while(!m_stopSignal.load())
    {
        std::shared_ptr<VideoDecHelper::RtpBuffer> rtp_buffer = m_video_dec_helper->getRtpVideoBuffer(min_buffer_size) ;

        size_t valid_bytes = recvIpcDataMore(rtp_buffer->raw_data, rtp_buffer->buffer_size);
        if (valid_bytes <= 0)
        {
            std::cerr << "recvIpcDataMore failed, buffer size may be too small" << std::endl;
            min_buffer_size *= 2;
            continue;
        }

        rtp_buffer->valid_data_bytes = valid_bytes;
        int ret = m_rtp_reader.parseHeader(rtp_buffer->raw_data.get(), valid_bytes, header);
        if (ret < 0)
        {
            std::cerr << "parseHeader failed" << std::endl;
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(m_rtp_info_mutex);
            m_rtp_info = header;
        }
        std::cout << "m_rtp_info.start_of_header: 0x" << std::hex << static_cast<int>(m_rtp_info.start_of_header) << std::endl;
        std::cout << "m_rtp_info.payload_type: 0x" << std::hex << static_cast<int>(m_rtp_info.payload_type) << std::endl;
        std::cout << "m_rtp_info.sequence_number: 0x" << std::hex << m_rtp_info.sequence_number << std::endl;
        std::cout << "m_rtp_info.timestamp: 0x" << std::hex << m_rtp_info.timestamp << std::endl;
        std::cout << "m_rtp_info.ssrc: 0x" << std::hex << m_rtp_info.ssrc << std::endl;
        std::cout << std::dec;

        rtp_buffer->payload = m_rtp_reader.extractPayload(rtp_buffer->raw_data.get(), valid_bytes);
        if (rtp_buffer->payload.size <= 0)
        {
            std::cerr << "extractPayload failed" << std::endl;
            continue;
        }
        rtp_buffer->payload_valid = true;
        m_video_dec_helper->sendToDecoder(rtp_buffer);
        rtp_buffer.reset();
        frame_count++;
        std::cout << "frame_count: " << frame_count << std::endl;
    }
    std::cout << "runLoop end" << std::endl;
}


} // namespace data_recorder
} // namespace apps
