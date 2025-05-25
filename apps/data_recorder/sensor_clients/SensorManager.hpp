#ifndef __SENSOR_MANAGER_HPP__
#define __SENSOR_MANAGER_HPP__

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "SensorClient.hpp"

using json = nlohmann::json;
namespace apps
{
namespace data_recorder
{

class SensorManager
{
public:
    explicit SensorManager(const json& sensors_array, const json& vehicle_info, const json& sensor_ipc);

    void runLoop();
    void stop()
    {
        m_stopSignal.store(true);
    }

    ~SensorManager();

private:
    std::vector<std::unique_ptr<SensorClient>> m_clients_list;
    std::atomic<bool> m_stopSignal{false};
};

} // namespace data_recorder
} // namespace apps
#endif // __SENSOR_MANAGER_HPP__
