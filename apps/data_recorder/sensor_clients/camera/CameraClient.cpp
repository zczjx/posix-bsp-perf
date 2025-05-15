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

    while(!m_stopSignal.load())
    {
        frame_count++;
        receiveTpcData(data);
        std::cout << "data size: " << data.size() << std::endl;
        std::cout << "frame_count: " << frame_count << std::endl;
    }
    std::cout << "runLoop end" << std::endl;
}


} // namespace data_recorder
} // namespace apps
