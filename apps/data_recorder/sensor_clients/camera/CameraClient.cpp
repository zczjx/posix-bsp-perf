#include "CameraClient.hpp"
#include <iostream>
namespace apps
{
namespace data_recorder
{

CameraClient::CameraClient(const json& sensor_context)
    : SensorClient(sensor_context)
{
    m_xres = sensor_context["image_size_x"];
    m_yres = sensor_context["image_size_y"];
    m_raw_pixel_format = sensor_context["raw_pixel_format"];

    m_main_thread = std::make_unique<std::thread>([this]() {runLoop();});
}

CameraClient::~CameraClient()
{
    if (m_main_thread->joinable())
    {
        m_stopSignal.store(true);
        m_main_thread->join();
    }
}

void CameraClient::runLoop()
{
    uint64_t frame_count = 0;
    std::cout << "runLoop" << std::endl;
    std::vector<uint8_t> data;
    std::vector<uint8_t> payload;

    while(!m_stopSignal.load())
    {
        RtpHeader header;
        frame_count++;
        receiveTpcDataMore(data);
        int ret = m_rtp_reader.parseHeader(data.data(), data.size(), header);
        if (ret < 0)
        {
            std::cerr << "parseHeader failed" << std::endl;
            continue;
        }
        std::cout << "header.vpxcc: 0x" << std::hex << static_cast<int>(header.vpxcc) << std::endl;
        std::cout << "header.mpt: 0x" << std::hex << static_cast<int>(header.mpt) << std::endl;
        std::cout << "header.sequence_number: 0x" << std::hex << header.sequence_number << std::endl;
        std::cout << "header.timestamp: 0x" << std::hex << header.timestamp << std::endl;
        std::cout << "header.ssrc: 0x" << std::hex << header.ssrc << std::endl;
        std::cout << std::dec;

        ret = m_rtp_reader.extractPayload(data.data(), data.size(), payload);

        if (ret < 0)
        {
            std::cerr << "extractPayload failed" << std::endl;
            continue;
        }
        std::cout << "frame_count: " << frame_count << std::endl;
    }
    std::cout << "runLoop end" << std::endl;
}


} // namespace data_recorder
} // namespace apps
