#include "CarlaVehicle.hpp"
#include <iostream>

namespace apps
{
namespace data_recorder
{

CarlaVehicle::CarlaVehicle(const json& rig_json, const std::string& output_file, const json& nodes_ipc)
    : m_rig_json(rig_json),
    m_output_file(output_file),
    m_nodes_ipc(nodes_ipc)
{
    m_sensor_manager = std::make_unique<SensorManager>(m_rig_json["sensors"], m_rig_json["vehicle"], m_nodes_ipc);
}

void CarlaVehicle::run()
{

    while (!m_stopSignal)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "CarlaVehicle::run" << std::endl;
        m_sensor_manager->runLoop();
    }

}

CarlaVehicle::~CarlaVehicle()
{
    m_stopSignal = true;
    m_sensor_manager->stop();
}

} // namespace data_recorder
} // namespace apps