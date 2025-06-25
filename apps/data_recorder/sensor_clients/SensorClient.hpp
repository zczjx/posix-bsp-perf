
#ifndef __SENSOR_CLIENT_HPP__
#define __SENSOR_CLIENT_HPP__

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <zeromq_ipc/zmqSubscriber.hpp>
#include <zeromq_ipc/sharedMemPublisher.hpp>

using json = nlohmann::json;
using namespace midware::zeromq_ipc;

namespace apps
{
namespace data_recorder
{

class SensorClient
{
public:
    explicit SensorClient(const json& sensor_context);

    virtual ~SensorClient() = default;

    virtual void runLoop() = 0;

    const std::string& getSensorName() const
    {
        return m_name;
    }

private:
    std::string m_type;
    std::string m_name;
};


} // namespace data_recorder
} // namespace apps

#endif // __SENSOR_CLIENT_HPP__