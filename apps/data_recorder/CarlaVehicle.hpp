#ifndef __CARLA_VEHICLE_HPP__
#define __CARLA_VEHICLE_HPP__

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "sensor_clients/SensorManager.hpp"

using json = nlohmann::json;

namespace apps
{
namespace data_recorder
{

class CarlaVehicle
{
public:
    explicit CarlaVehicle(const json& rig_json, const std::string& output_file);

    void run();

    void stop()
    {
        m_sensor_manager->stop();
        m_stopSignal = true;
    }

    ~CarlaVehicle();

private:
    json m_rig_json;
    std::string m_output_file;
    std::unique_ptr<SensorManager> m_sensor_manager;
    std::atomic<bool> m_stopSignal{false};
};

} // namespace data_recorder
} // namespace apps

#endif // __CARLA_VEHICLE_HPP__