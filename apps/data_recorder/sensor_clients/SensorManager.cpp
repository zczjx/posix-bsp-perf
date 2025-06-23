
#include "SensorManager.hpp"
#include "camera/CameraClient.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace apps
{
namespace data_recorder
{

SensorManager::SensorManager(const json& sensors_array, const json& vehicle_info, const json& node_ipc)
{
    for (const auto& sensor: sensors_array)
    {
        std::string sensor_type = sensor["type"];

        if (sensor_type.compare("camera") == 0)
        {
            std::string sensor_name = sensor["name"];
            for (const auto& camera_node: node_ipc["camera"])
            {
                std::string camera_node_name = camera_node["name"];
                if (camera_node_name.compare(sensor_name) == 0)
                {
                    std::cout << "Camera client created" << std::endl;
                    m_clients_list.push_back(std::make_unique<CameraClient>(sensor, vehicle_info, camera_node));
                    break;
                }
            }
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