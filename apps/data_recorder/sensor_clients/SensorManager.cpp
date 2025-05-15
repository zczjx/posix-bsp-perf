
#include "SensorManager.hpp"
#include "camera/CameraClient.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

SensorManager::SensorManager(const json& sensors_array)
{
    for (const auto& sensor: sensors_array)
    {
        std::string sensor_type = sensor["type"];

        if (sensor_type.compare("camera") == 0)
        {
            std::cout << "Camera client created" << std::endl;
            m_clients_list.push_back(std::make_unique<CameraClient>(sensor));
        }
    }
}

void SensorManager::runLoop()
{
    size_t cycle_count = 0;

    while (!m_stopSignal.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        cycle_count++;
        std::cout << "Cycle count: " << cycle_count << std::endl;
    }
}

SensorManager::~SensorManager()
{
    m_stopSignal.store(true);
    m_clients_list.clear();
}

} // namespace data_recorder
} // namespace apps