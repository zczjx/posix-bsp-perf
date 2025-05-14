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

    while(!m_stopSignal.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        frame_count++;
    }
    std::cout << "runLoop end" << std::endl;
}


} // namespace data_recorder
} // namespace apps
